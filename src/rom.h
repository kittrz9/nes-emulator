#ifndef ROM_H
#define ROM_H

#include <stdint.h>

// https://www.nesdev.org/wiki/INES
typedef struct {
	uint8_t NES[4];
	uint8_t prgSize; // 16kb clusters
	uint8_t chrSize; // 8kb clusters
	uint8_t flags6;
	uint8_t flags7;
	uint8_t flags8;
	uint8_t flags9;
	uint8_t flags10;
	uint8_t padding[5];
} iNESHeader;

uint8_t loadROM(const char* path);

#endif
