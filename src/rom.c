#include "rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "ram.h"
#include "ppu.h"
#include "cart.h"

uint8_t loadROM(const char* path) {
	FILE* f = fopen(path, "rb");
	if(!f) {
		printf("could not find file \"%s\"\n", path);
		return 1;
	}

	size_t fileSize;
	fseek(f, 0, SEEK_END);
	fileSize = ftell(f);
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
		printf("NES 2.0 rom, unsupported\n");
		return 1;
	}

	if(header->flags6 & 0x02) {
		printf("battery backed PRG RAM, unsupported\n");
		return 1;
	}

	printf("PRG ROM size: %uk\n", header->prgSize*16);
	printf("CHR ROM size: %uk\n", header->chrSize*8);
	uint8_t mapperID = ((header->flags6 & 0xF0) >> 4) | ((header->flags7 & 0xF0) << 4);
	printf("mapper ID: %02X\n", mapperID);

	if(mapperID != 0) {
		printf("mappers besides NROM are unsupported\n");
		return 1;
	}

	uint8_t* prgLocation = fileBuffer+sizeof(iNESHeader);
	if(header->flags6 & 0x04) {
		printf("trainer in rom\n");
		prgLocation += 512;
	}
	uint8_t* chrLocation = prgLocation + 0x4000*header->prgSize;

	size_t prgSize = header->prgSize * 0x4000;
	size_t chrSize = header->chrSize * 0x2000;

	prgROM = malloc(prgSize);
	chrROM = malloc(chrSize);

	memcpy(prgROM, prgLocation, prgSize);
	memcpy(chrROM, chrLocation, chrSize);

	free(fileBuffer);

	/*f = fopen("test.prg", "wb");
	if(!f) {
		return 1;
	}
	fwrite(prgROM, prgSize, 1, f);
	fclose(f);

	f = fopen("test.chr", "wb");
	if(!f) {
		return 1;
	}
	fwrite(chrROM, chrSize, 1, f);
	fclose(f);*/

	if(prgSize > 0x4000) {
		memcpy(cpuRAM+0x8000, prgROM, 0x8000);
	} else {
		memcpy(cpuRAM+0x8000, prgROM, 0x4000);
		memcpy(cpuRAM+0xC000, prgROM, 0x4000);
	}

	memcpy(ppuRAM, chrROM, 0x4000);

	/*for(uint32_t i = 0; i < 0x10000; ++i) {
		if(i % 0x10 == 0) {
			printf("\n%04X ", i);
		}
		printf("%02X", cpuRAM[i]);
	}*/
	printf("\n\n");

	return 0;
}
