#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

typedef struct {
	uint8_t buttons;
	uint8_t currentBit;
} controller_t;

extern uint8_t controllerLatch;

extern controller_t controllers[2];

uint8_t pollController(uint8_t port);

#endif
