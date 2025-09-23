#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#define NMI_VECTOR 0xFFFA
#define RST_VECTOR 0xFFFC
#define IRQ_VECTOR 0xFFFE

// https://www.nesdev.org/wiki/Status_flags
#define C_FLAG 0x01
#define Z_FLAG 0x02
#define I_FLAG 0x04
#define D_FLAG 0x08
#define B_FLAG 0x10
//#define ONE_FLAG 0x20
#define V_FLAG 0x40
#define N_FLAG 0x80

typedef struct {
	uint8_t a;
	uint8_t x;
	uint8_t y;
	uint16_t pc;
	uint8_t s;
	uint8_t p;
	uint8_t irq;
	uint64_t cycles;
} cpu_t;

extern cpu_t cpu;

void push(uint8_t byte);
uint8_t pop(void);

void cpuInit(void);
uint8_t cpuStep(void);

void cpuDumpState(void);

#endif
