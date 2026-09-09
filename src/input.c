#include "input.h"

#include "SDL3/SDL.h"

#include "ppu.h"
#include "cpu.h"
#include "ram.h"

#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

controller_t controllers[2];

uint8_t controllerLatch;
int keyNumber;
const uint8_t* keys;
uint8_t* keysLastFrame;

SDL_Event e;

// https://fceux.com/web/help/fm2.html
typedef struct {
	uint8_t command; // 1 for soft reset, 2 for hard reset
	uint8_t port0;
	uint8_t port1;
	// ignoring port2, doesn't seem to be used for anything
} tasInput;

//tasInput* tas;
#define TAS_INITIAL_SIZE 32
struct {
	tasInput* inputs; // probably should rename something to avoid confusing tas.inputs and tasInput
	size_t size;
	uint32_t currentFrame; // could also probably be renamed if this ever does subframe tas stuff
	uint32_t maxFrame;
} tas;

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
	if(tas.inputs == NULL) {
		for(uint8_t i = 0; i < 8; ++i) {
			if(keys[inputKeys[i]]) {
				controllers[0].buttons |= 1 << i;
			} else {
				controllers[0].buttons &= ~(1 << i);
			}
		}
	} else {
		drawDebugText(0, 200, "%i/%i", tas.currentFrame, tas.maxFrame);
		drawDebugText(0, 100, "%02X,%02X", tas.inputs[tas.currentFrame].port0, tas.inputs[tas.currentFrame].port1);
		if(tas.currentFrame < tas.maxFrame) {
			switch(tas.inputs[tas.currentFrame].command) {
				case 0: break;
				case 1: // soft reset
					cpu.pc = ADDR16(RST_VECTOR);
					break;
				case 2: // hard reset
					cpuInit();
					break;
				default: // unimplemented
					break;
			}
			controllers[0].buttons = tas.inputs[tas.currentFrame].port0;
			controllers[1].buttons = tas.inputs[tas.currentFrame].port1;
			++tas.currentFrame;
		} else {
			controllers[0].buttons = 0;
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

void addTasInput(tasInput input) {
	if(tas.maxFrame == tas.size) {
		if(tas.size == 0) {
			tas.size = TAS_INITIAL_SIZE;
		} else {
			tas.size *= 2;
		}
		tas.inputs = realloc(tas.inputs, tas.size * sizeof(tasInput));
	}
	tas.inputs[tas.maxFrame] = input;
	++tas.maxFrame;
}

#define TAS_MAX_LINE_LENGTH 64
uint8_t loadTas(char* tasPath) {
	// maybe could get away with not reading in the full tas file at once and just fgetsing it for each input poll,
	// but that'd probably be really slow lmao
	FILE* tasFile = fopen(tasPath, "rb");
	char lineStr[TAS_MAX_LINE_LENGTH];
	fgets(lineStr, TAS_MAX_LINE_LENGTH, tasFile);
	// this breaks if there's more spaces between "version" and "3", which I think the format allows
	if(strcmp(lineStr, "version 3\n") != 0) {
		printf("%s\n", lineStr);
		printf("invalid tas format or unsupported format version\n");
		return 1;
	}
	// ignoring header stuff for now, should at least check port stuff and if it's using the binary format eventually
	while(lineStr != NULL && lineStr[0] != '|') {
		fgets(lineStr, TAS_MAX_LINE_LENGTH, tasFile);
	}
	if(lineStr == NULL) {
		return 1;
	}
	// praying the input format isn't binary for now
	// also hoping it's just a normal controller tas and not like zapper stuff
	while(lineStr[0] == '|') {
		tasInput currentInput;
		char temp[32] = "";

		size_t i = 0;
		char* c = lineStr+1;
		while(*c != '|' && *c != '\n' && *c != '\0') {
			temp[i] = *c;
			++i;
			++c;
		}
		if(*c != '|') {
			printf("invalid tas input\n");
			return 1;
		}
		++i;
		temp[i] = '\0';
		currentInput.command = atoi(temp);

		++c;
		currentInput.port0 = 0;
		for(i = 0; i < 8; ++i) {
			if(*c != '.' && *c != ' ') {
				currentInput.port0 |= 0x80 >> i;
			}
			++c;
			if(*c == '|' || *c == '\n' || *c == '\0') { break; } // probably not something that actually happens in normal input but whatever
		}
		if(*c != '|') {
			printf("invalid tas input\n");
			return 1;
		}

		++c;
		currentInput.port1 = 0;
		for(i = 0; i < 8; ++i) {
			if(*c != '.' && *c != ' ') {
				currentInput.port1 |= 0x80 >> i;
			}
			++c;
			if(*c == '|' || *c == '\n' || *c == '\0') { break; } // probably not something that actually happens in normal input but whatever
		}
		if(*c != '|') {
			printf("invalid tas input\n");
			return 1;
		}

		// still ignoring port2 stuff

		addTasInput(currentInput);

		if(fgets(lineStr, TAS_MAX_LINE_LENGTH, tasFile) == NULL) {
			break;
		}
	}
	fclose(tasFile);
	// probably should free the tas stuff when it exits but it's probably not a big deal to leave it for now
}
