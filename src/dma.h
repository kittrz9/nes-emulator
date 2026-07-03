#ifndef DMA_H
#define DMA_H

#include <stdint.h>

enum {
	DMA_CYCLE_PUT = 0,
	DMA_CYCLE_GET = 1,
};

extern uint8_t dmaCycle;


extern uint8_t dmaActive;

void dmaStep(void);
void oamDMAStart(uint8_t page);


#endif // DMA_H
