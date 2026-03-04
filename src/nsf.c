#include "nsf.h"

#include <SDL3/SDL.h>

#include "ram.h"
#include "cpu.h"
#include "input.h"
#include "rom.h"
#include "ppu.h"
#include "apu.h"
#include "debug.h"

void nsfInit(uint8_t song) {
	// https://www.nesdev.org/wiki/NSF#Initializing_a_tune
	toggleFPSCap(); // probably would be better to just not deal with the already existing rendering code in ppu.c
	// could use memset here, who cares
	for(uint16_t i = 0; i < 0x800; ++i) {
		ramWriteByte(i, 0);
	}

	for(uint16_t i = 0x4000; i < 0x4014; ++i) {
		ramWriteByte(i, 0);
	}
	cpu.irq = 1;
	cpu.nmi = 1;
	ramWriteByte(0x4015, 0);
	ramWriteByte(0x4015, 0xF);
	ramWriteByte(0x4017, 0x40);
	cpu.a = song;
	cpu.x = 0;
	push(0);
	push(0);
	cpu.pc = rom.nsfInitAddr;
	while(cpu.pc != 1) { // rts puts it 1 byte ahead of the address pushed to the stack
		cpuStep();
	}
}

int nsfMain(void) {
	push(0);
	push(0);
	cpu.pc = rom.nsfPlayAddr;
	uint64_t rate = 1000000/rom.nsfSpeed;
	uint64_t timerPeriod = 1789773/rate;
	int64_t timer = timerPeriod;
	//printf("%lu %lu\n", timerPeriod, rate);
	cpu.cycles = 0;
	while(1) {
		while(cpu.pc != 1) {
			cpuStep();
			for(uint8_t i = 0; i < cpu.cycles; ++i) {
				apuStep();
			}
			timer -= cpu.cycles;
			cpu.cycles = 0;
		}
		//printf("balls %i\n", timer);
		if(timer > 0) {
			for(size_t i = 0; i < timer; ++i) {
				apuStep();
			}
		}
		timer = timerPeriod;
		push(0);
		push(0);
		cpu.pc = rom.nsfPlayAddr;

		if(handleInput() != 0) { return 1; }
		drawDebugText(0, 0, "song: %s\nauthor: %s", rom.nsfSongName, rom.nsfSongAuthor);
		render();

		static uint64_t t1 = 0;
		if(t1 == 0) {
			t1 = SDL_GetTicksNS();
		}
		uint64_t t2 = SDL_GetTicksNS();
		//printf("%lu %lu\n", t2-t1, 1000000000/rate);
		if(t2 - t1 < 1000000000/rate) {
			SDL_DelayNS(1000000000/rate - (t2 - t1));
		}
		t1 = SDL_GetTicksNS();
	}
	return 0;
}
