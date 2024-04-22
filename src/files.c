#include "files.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "ram.h"
#include "ppu.h"
#include "rom.h"

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

	iNESHeader* header = (iNESHeader*)fileBuffer;

	if(strncmp((char*)header->NES, "NES\x1A", 4) != 0) {
		printf("invalid header\n");
		return 1;
	}

	if((header->flags7 & 0x0C) == 0x08) {
		printf("NES 2.0 rom\n");

	}

	if(header->flags6 & 0x02) {
		printf("battery backed PRG RAM, unsupported\n");
	}

	printf("PRG ROM size: %uk\n", header->prgSize*16);
	printf("CHR ROM size: %uk\n", header->chrSize*8);
	uint8_t mapperID = ((header->flags6 & 0xF0) >> 4) | ((header->flags7 & 0xF0));
	printf("mapper ID: %02X\n", mapperID);

	setMapper(mapperID);

	uint8_t* prgLocation = fileBuffer+sizeof(iNESHeader);
	if(header->flags6 & 0x04) {
		printf("trainer in rom\n");
		prgLocation += 512;
	}

	prgSize = header->prgSize * 0x4000;
	chrSize = header->chrSize * 0x2000;
	uint8_t* chrLocation = prgLocation + prgSize;

	prgROM = malloc(prgSize);
	chrROM = malloc(chrSize);

	memcpy(prgROM, prgLocation, prgSize);
	if(chrSize != 0) {
		memcpy(chrROM, chrLocation, chrSize);
		memcpy(ppuRAM, chrROM, 0x2000);
	}

	ppu.mirror = header->flags6 & 0x1;
	printf("mirror: %02X", ppu.mirror);

	free(fileBuffer);


	printf("\n\n");

	return 0;
}
