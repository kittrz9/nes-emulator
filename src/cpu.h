#ifndef CPU_H
#define CPU_H

#include <stdint.h>

typedef struct {
	uint8_t a;
	uint8_t x;
	uint8_t y;
	uint16_t pc;
	uint8_t s;
	uint8_t p;
} cpu_t;

extern cpu_t cpu;

void cpuInit();
uint8_t cpuStep();

#endif
