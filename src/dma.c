#include "dma.h"

#include "cpu.h"
#include "ram.h"

uint8_t dmaCycle;

uint8_t dmaActive;


// oam dma
uint8_t oamPage;
uint16_t oamIndex;
uint8_t retrievedOamByte;

// will probably implement dmc dma with this too

void dmaStep(void) {
	if(dmaCycle == DMA_CYCLE_PUT) {
		ramWriteByte(0x2004, retrievedOamByte);
		++oamIndex;
		if(oamIndex > 255) {
			dmaActive = 0;
		}
	} else {
		retrievedOamByte = ramReadByte((oamPage << 8) + oamIndex);
	}
	++cpu.cycles;
}

void oamDMAStart(uint8_t page) {
	oamPage = page;
	oamIndex = 0;
	dmaActive = 1; // honestly I don't remember why I'm not using stdbool lmao, I think I just got tired of including it over and over again
	if(dmaCycle == DMA_CYCLE_PUT) {
		++cpu.cycles;
	}
}
