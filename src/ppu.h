#ifndef PPU_H
#define PPU_H

#include <stdint.h>

#define FB_WIDTH 256
#define FB_HEIGHT 240

#define SCREEN_WIDTH FB_WIDTH*2
#define SCREEN_HEIGHT FB_HEIGHT*2

#define MIRROR_VERTICAL 0
#define MIRROR_HORIZONTAL 1

// https://www.nesdev.org/wiki/Cycle_reference_chart
#define CYCLES_PER_FRAME 29780
#define CYCLES_PER_VBLANK 2273
#define CYCLES_PER_SCANLINE 114

#define PPU_STATUS_SPRITE_OVERFLOW 0x20
#define PPU_STATUS_SPRITE_0 0x40
#define PPU_STATUS_VBLANK 0x80

#define PPU_MASK_GRAY 0x01
#define PPU_MASK_LEFT_BACKGROUND 0x02
#define PPU_MASK_LEFT_SPRITES 0x04
#define PPU_MASK_ENABLE_BACKGROUND 0x08
#define PPU_MASK_ENABLE_SPRITES 0x10
#define PPU_MASK_EMPH_RED 0x20
#define PPU_MASK_EMPH_GREEN 0x40
#define PPU_MASK_EMPH_BLUE 0x80

#define PPU_CTRL_VRAM_INC_DIR 0x04
#define PPU_CTRL_SPRITE_TABLE 0x08
#define PPU_CTRL_BACKGROUND_TABLE 0x10
#define PPU_CTRL_SPRITE_SIZE 0x20
#define PPU_CTRL_READ_EXT 0x40
#define PPU_CTRL_ENABLE_VBLANK 0x80

#define PPU_OAM_PRIORITY 0x20
#define PPU_OAM_FLIP_HORIZONTAL 0x40
#define PPU_OAM_FLIP_VERTICAL 0x80
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
	uint8_t readBuffer;

	uint16_t currentPixel;
} ppu_t;

extern ppu_t ppu;

extern uint8_t ppuRAM[0x4000];

extern uint8_t nametableBank1[0x400];
extern uint8_t nametableBank2[0x400];

uint8_t initRenderer(void);
void uninitRenderer(void);

void debugScreenshot(void);

void toggleFPSCap(void);

void drawPixel(uint16_t x, uint16_t y);
void render(void);

#endif
