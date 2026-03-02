#ifndef ROM_H
#define ROM_H

#include <stdint.h>
#include <stddef.h>


typedef struct rom_t {
	uint8_t* prgROM;
	uint8_t* chrROM;
	size_t prgSize;
	size_t chrSize;
	uint8_t prgRAMEnabled;

	uint8_t isNSF;
	uint16_t nsfLoadAddr;
	uint16_t nsfInitAddr;
	uint16_t nsfPlayAddr;
	uint16_t nsfSpeed;
	char nsfSongName[32];
	char nsfSongAuthor[32];
	char nsfSongCopyright[32];
} rom_t;

extern rom_t rom;

void setMapper(uint16_t id);

// probably should get better names for these that aren't so similar to the ram functions
extern void (*romWriteByte)(uint16_t addr, uint8_t byte);
extern uint8_t (*romReadByte)(uint16_t addr);

extern void (*chrWriteByte)(uint16_t addr, uint8_t byte);
extern uint8_t (*chrReadByte)(uint16_t addr);

extern void (*scanlineCounter)(void);
extern void (*cycleCounter)(void);

extern float (*expandedAudioGetSample)(void);

#endif
