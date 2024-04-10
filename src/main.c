#include <stdio.h>
#include <stdint.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

#include "rom.h"
#include "ram.h"
#include "cart.h"
#include "cpu.h"
#include "ppu.h"
#include "input.h"

// https://www.nesdev.org/wiki/Cycle_reference_chart
#define CYCLES_PER_FRAME 29780
#define CYCLES_PER_VBLANK 2273

int main(int argc, char** argv) {
	if(argc < 2) {
		printf("usage: %s romPath\n", argv[0]);
		return 1;
	}

	if(loadROM(argv[1]) != 0) {
		return 1;
	}

	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("could not init SDL\n");
		return 1;
	}

	initRenderer();

	cpuInit();

	int keyNumber;
	const uint8_t* keys = SDL_GetKeyboardState(&keyNumber);

	SDL_Event e;
	while(1) {
		SDL_PollEvent(&e);
		if(e.type == SDL_QUIT) {
			break;
		}
		const SDL_Keycode inputKeys[] = {
			SDL_SCANCODE_RIGHT,
			SDL_SCANCODE_LEFT,
			SDL_SCANCODE_DOWN,
			SDL_SCANCODE_UP,
			SDL_SCANCODE_RETURN,
			SDL_SCANCODE_RSHIFT,
			SDL_SCANCODE_X,
			SDL_SCANCODE_Z,
		};
		for(uint8_t i = 0; i < 8; ++i) {
			if(keys[inputKeys[i]]) {
				controllers[0].buttons |= 1 << i;
			} else {
				controllers[0].buttons &= ~(1 << i);
			}
		}
		//controllers[0].buttons |= 1;

		while(cpu.cycles <= CYCLES_PER_FRAME - CYCLES_PER_VBLANK) {
			cpuStep();
		}
		printf("funny vblank\n");
		cpu.cycles = 0;
		ppu.status |= 0x80;
		if(ppu.control & 0x80) {
			push(cpu.pc & 0xFF);
			push((cpu.pc & 0xFF00) >> 8);
			push(cpu.p);
			cpu.pc = ADDR16(NMI_VECTOR);
		}

		while(cpu.cycles <= CYCLES_PER_VBLANK) {
			cpuStep();
		}
		//printf("vblank over\n");
		ppu.status &= ~(0x80);
		cpu.cycles = 0;

		render();
	};

	free(prgROM);
	free(chrROM);

	SDL_Quit();
	
	return 0;
}
