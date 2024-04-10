#include "input.h"

#include <stdio.h>

controller_t controllers[2];

uint8_t controllerLatch;

uint8_t pollController(uint8_t port) {
	controller_t* c = &controllers[port];

	if(controllerLatch) { return 0; } // idk what happens when you try to read while latch is set, presumably it reads like just open bus stuff

	if(c->currentBit == 0) { c->currentBit = 0x80; }
	uint8_t ret = (c->buttons & c->currentBit) != 0;
	c->currentBit >>= 1;
	//printf("controller polled: %02X\n", ret);
	return ret;
}

