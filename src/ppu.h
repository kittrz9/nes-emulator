#ifndef PPU_H
#define PPU_H

#include <stdint.h>

//https://www.nesdev.org/wiki/PPU_registers
typedef struct {
	uint8_t control;
	uint8_t mask;
	uint8_t status;
	uint16_t oamAddr;
	uint16_t vramAddr;
	uint8_t t;
	uint8_t x;
	uint8_t w;
	uint8_t oam[256];
} ppu_t;

extern ppu_t ppu;

extern uint8_t ppuRAM[0x4000];

#endif
