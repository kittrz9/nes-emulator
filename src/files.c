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

// https://www.nesdev.org/wiki/NSF
typedef struct {
	uint8_t NESM[5]; // "NESM" 0x1A
	uint8_t version;
	uint8_t totalSongs;
	uint8_t startSong;
	uint8_t dataLoadLow;
	uint8_t dataLoadHigh;
	uint8_t dataInitLow;
	uint8_t dataInitHigh;
	uint8_t dataPlayLow;
	uint8_t dataPlayHigh;
	uint8_t songName[32];
	uint8_t songAuthor[32];
	uint8_t songCopyright[32];
	uint8_t playSpeedLow;
	uint8_t playSpeedHigh;
	uint8_t bankSwitchValues[8];
	uint8_t region;
	uint8_t audioExpansion;
	uint8_t reserved;
	uint8_t dataLength;
} nsfHeader;

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

	if(strncmp((char*)fileBuffer, "NESM\x1A", 5) == 0) {
		// nsf
		rom.isNSF = 1;
		nsfHeader* header = (nsfHeader*)fileBuffer;
		for(uint8_t i = 0; i < 8; ++i) {
			if(header->bankSwitchValues[i] != 0) {
				printf("nsf bankswitching unimplemented\n");
				exit(1);
			}
		}
		printf("song name: %s\n", header->songName);
		printf("author name: %s\n", header->songAuthor);

		memcpy(rom.nsfSongName, header->songName, 32);
		memcpy(rom.nsfSongAuthor, header->songAuthor, 32);
		memcpy(rom.nsfSongCopyright, header->songCopyright, 32);
		rom.prgSize = 0x8000;
		rom.chrSize = 0x2000;
		rom.prgROM = malloc(rom.prgSize);
		rom.chrROM = malloc(rom.chrSize);
		rom.nsfLoadAddr = (header->dataLoadHigh << 8) | header->dataLoadLow;
		rom.nsfInitAddr = (header->dataInitHigh << 8) | header->dataInitLow;
		rom.nsfPlayAddr = (header->dataPlayHigh << 8) | header->dataPlayLow;
		rom.nsfSpeed = (header->playSpeedHigh << 8) | header->playSpeedLow;

		memcpy(rom.prgROM + rom.nsfLoadAddr - 0x8000, fileBuffer+0x80, fileSize-0x80);
		setMapper(0);

		free(fileBuffer);
		return 0;
	}

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
	rom.prgSize = 0;
	rom.chrSize = 0;
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
			rom.prgSize = (1<<exponent) * mult;
		} else {
			rom.prgSize = (((header->romSizeMSB & 0xF) << 8) | header->prgSizeLSB) * 0x4000;  
		}

		if((header->romSizeMSB & 0xF0) == 0xF0) {
			// exponent notation
			uint8_t mult = (header->chrSizeLSB & 0x3)*2 + 1;
			uint8_t exponent = header->chrSizeLSB >> 2;
			rom.chrSize = (1<<exponent) * mult;
		} else {
			// already shifted by 4
			rom.chrSize = (((header->romSizeMSB & 0xF0) << 4) | header->chrSizeLSB) * 0x2000;  
		}

		if((header->chrRAMSize & 0xF) != 0) {
			chrRAMSize = 64 << (header->chrRAMSize & 0xF);
		}


		// still need to implement the msb for mapper ids and submapper ids
		mapperID = ((header->flags6 & 0xF0) >> 4) | ((header->flags7 & 0xF0));
	} else {
		iNESHeader* header = (iNESHeader*)fileBuffer;

		mapperID = ((header->flags6 & 0xF0) >> 4) | ((header->flags7 & 0xF0));

		rom.prgSize = header->prgSize * 0x4000;
		rom.chrSize = header->chrSize * 0x2000;

		if(rom.chrSize == 0) {
			chrRAMSize = 0x2000;
		}
	}
	printf("PRG ROM size: %luk\n", rom.prgSize / 0x400);
	printf("CHR ROM size: %luk\n", rom.chrSize / 0x400);
	printf("CHR RAM size: %luk\n", chrRAMSize / 0x400);
	printf("mirror: %02X\n", ppu.mirror);
	printf("mapper ID: %02X\n", mapperID);

	setMapper(mapperID);

	prgLocation = fileBuffer+16;
	if(fileBuffer[6] & 0x04) {
		printf("trainer in rom\n");
		prgLocation += 512;
	}
	chrLocation = prgLocation + rom.prgSize;

	if(rom.prgSize != 0) {
		rom.prgROM = malloc(rom.prgSize);
		memcpy(rom.prgROM, prgLocation, rom.prgSize);
	}
	if(rom.chrSize != 0) {
		rom.chrROM = malloc(rom.chrSize);
		memcpy(rom.chrROM, chrLocation, rom.chrSize);
	} else if(chrRAMSize != 0) {
		rom.chrROM = malloc(chrRAMSize); // there's probably some things that bank switch between chr rom and chr ram, this needs to be fixed
	}

	free(fileBuffer);


	printf("\n\n");

	return 0;
}
