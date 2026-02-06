#include "files.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "ram.h"
#include "ppu.h"
#include "rom.h"

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

// https://www.nesdev.org/wiki/NES_2.0
typedef struct {
	uint8_t NES[4];
	uint8_t prgSizeLSB;
	uint8_t chrSizeLSB;
	uint8_t flags6;
	uint8_t flags7;
	uint8_t mapperMSB;
	uint8_t romSizeMSB;
	uint8_t prgRAMSize;
	uint8_t chrRAMSize;
	uint8_t timing;
	uint8_t flags13;
	uint8_t miscROMs;
	uint8_t expansionDevice;
} nes2Header;

uint8_t loadROM(const char* path) {
	FILE* f = fopen(path, "rb");
	if(!f) {
		printf("could not find file \"%s\"\n", path);
		return 1;
	}

	fseek(f, 0, SEEK_END);
	size_t fileSize = ftell(f);
	printf("%lu\n", fileSize);
	fseek(f, 0, SEEK_SET);

	uint8_t* fileBuffer = malloc(fileSize);

	fread(fileBuffer, fileSize, 1, f);

	fclose(f);

	if(strncmp((char*)fileBuffer, "NES\x1A", 4) != 0) {
		printf("invalid header\n");
		return 1;
	}

	// common between formats
	ppu.mirror = fileBuffer[6] & 0x1;
	if(fileBuffer[6] & 0x02) {
		printf("battery backed PRG RAM, unsupported\n");
	}

	uint8_t* prgLocation;
	uint8_t* chrLocation;
	prgSize = 0;
	chrSize = 0;
	size_t chrRAMSize = 0;
	uint16_t mapperID;

	if((fileBuffer[7] & 0x0C) == 0x08) {
		printf("NES 2.0 rom\n");
		nes2Header* header = (nes2Header*)fileBuffer;

		printf("timing: %02X\n", header->timing & 0x3);

		if((header->romSizeMSB & 0xF) == 0xF) {
			// exponent notation
			uint8_t mult = (header->prgSizeLSB & 0x3)*2 + 1;
			uint8_t exponent = header->prgSizeLSB >> 2;
			prgSize = (1<<exponent) * mult;
		} else {
			prgSize = (((header->romSizeMSB & 0xF) << 8) | header->prgSizeLSB) * 0x4000;  
		}

		if((header->romSizeMSB & 0xF0) == 0xF0) {
			// exponent notation
			uint8_t mult = (header->chrSizeLSB & 0x3)*2 + 1;
			uint8_t exponent = header->chrSizeLSB >> 2;
			chrSize = (1<<exponent) * mult;
		} else {
			// already shifted by 4
			chrSize = (((header->romSizeMSB & 0xF0) << 4) | header->chrSizeLSB) * 0x2000;  
		}

		if((header->chrRAMSize & 0xF) != 0) {
			chrRAMSize = 64 << (header->chrRAMSize & 0xF);
		}


		// still need to implement the msb for mapper ids and submapper ids
		mapperID = ((header->flags6 & 0xF0) >> 4) | ((header->flags7 & 0xF0));
	} else {
		iNESHeader* header = (iNESHeader*)fileBuffer;

		mapperID = ((header->flags6 & 0xF0) >> 4) | ((header->flags7 & 0xF0));

		prgSize = header->prgSize * 0x4000;
		chrSize = header->chrSize * 0x2000;

		if(chrSize == 0) {
			chrRAMSize = 0x2000;
		}
	}
	printf("PRG ROM size: %luk\n", prgSize / 0x400);
	printf("CHR ROM size: %luk\n", chrSize / 0x400);
	printf("CHR RAM size: %luk\n", chrRAMSize / 0x400);
	printf("mirror: %02X\n", ppu.mirror);
	printf("mapper ID: %02X\n", mapperID);

	setMapper(mapperID);

	prgLocation = fileBuffer+16;
	if(fileBuffer[6] & 0x04) {
		printf("trainer in rom\n");
		prgLocation += 512;
	}
	chrLocation = prgLocation + prgSize;

	if(prgSize != 0) {
		prgROM = malloc(prgSize);
		memcpy(prgROM, prgLocation, prgSize);
	}
	if(chrSize != 0) {
		chrROM = malloc(chrSize);
		memcpy(chrROM, chrLocation, chrSize);
	} else if(chrRAMSize != 0) {
		chrROM = malloc(chrRAMSize); // there's probably some things that bank switch between chr rom and chr ram, this needs to be fixed
	}

	free(fileBuffer);


	printf("\n\n");

	return 0;
}
