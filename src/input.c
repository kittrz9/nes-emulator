#include "input.h"

#include "SDL3/SDL.h"

#include "ppu.h"
#include "cpu.h"
#include "ram.h"

#include "debug.h"

#include <stdio.h>
#include <stdlib.h>

controller_t controllers[2];

uint8_t controllerLatch;
int keyNumber;
const uint8_t* keys;
uint8_t* keysLastFrame;

SDL_Event e;

uint8_t pollController(uint8_t port) {
	controller_t* c = &controllers[port];

	if(controllerLatch) {
		c->shiftRegister = c->buttons;
		return c->shiftRegister & 0x1;
	}

	uint8_t ret = c->shiftRegister & 1;
	c->shiftRegister >>= 1;
	c->shiftRegister |= 0x80;
	return ret;
}

void initInput(void) {
	keys = (const uint8_t*)SDL_GetKeyboardState(&keyNumber); 
	keysLastFrame = malloc(sizeof(uint8_t) * keyNumber); // memory leak of like maybe 200 bytes because I don't care enough to free it lmao
	memset(keysLastFrame, 0, sizeof(uint8_t) * keyNumber);
}

uint8_t handleInput(void) {
	SDL_PollEvent(&e);
	if(e.type == SDL_EVENT_QUIT) {
		return 1;
	}
	const SDL_Keycode inputKeys[] = {
		SDL_SCANCODE_Z, // A
		SDL_SCANCODE_X, // B
		SDL_SCANCODE_RSHIFT, // SELECT
		SDL_SCANCODE_RETURN, // START
		SDL_SCANCODE_UP,
		SDL_SCANCODE_DOWN,
		SDL_SCANCODE_LEFT,
		SDL_SCANCODE_RIGHT,
	};
	for(uint8_t i = 0; i < 8; ++i) {
		if(keys[inputKeys[i]]) {
			controllers[0].buttons |= 1 << i;
		} else {
			controllers[0].buttons &= ~(1 << i);
		}
	}

	if(keys[SDL_SCANCODE_P]) {
		debugScreenshot();
		exit(1);
	}

	if(keys[SDL_SCANCODE_D] && !keysLastFrame[SDL_SCANCODE_D]) {
		toggleDebugInfo();
	}

	if(keys[SDL_SCANCODE_F1] && !keysLastFrame[SDL_SCANCODE_F1]) {
		cpuDumpState();
	}

	if(keys[SDL_SCANCODE_TAB] && !keysLastFrame[SDL_SCANCODE_TAB]) {
		toggleFPSCap();
	}


	if(keys[SDL_SCANCODE_R]) {
		cpu.pc = ADDR16(RST_VECTOR);
	}

	memcpy(keysLastFrame, keys, sizeof(uint8_t) * keyNumber);
	SDL_PumpEvents();

	return 0;
}
