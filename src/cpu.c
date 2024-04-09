#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>

#include "ram.h"

cpu_t cpu;

#define NMI_VECTOR 0xFFFA
#define RST_VECTOR 0xFFFC
#define IRQ_VECTOR 0xFFFE

// https://www.nesdev.org/wiki/Status_flags
#define C_FLAG 0x01
#define Z_FLAG 0x02
#define I_FLAG 0x04
#define D_FLAG 0x08
//#define B_FLAG 0x10
//#define ONE_FLAG 0x20
#define V_FLAG 0x40
#define N_FLAG 0x80

void cpuInit() {
	cpu.pc = ADDR16(RST_VECTOR);
	return;
}

uint8_t cpuStep() {
	uint8_t opcode = cpuRAM[cpu.pc];

	printf("pc: %04X\n", cpu.pc);
	printf("a: %02X, x: %02X, y: %02X\n", cpu.a, cpu.x, cpu.y);
	printf("p: %02X\n", cpu.p);
	printf("opcode: %02X\n", opcode);

	// https://www.masswerk.at/6502/6502_instruction_set.html
	// https://www.nesdev.org/obelisk-6502-guide/reference.html
	switch(opcode) {
		// BPL
		case 0x10:
			if((cpu.p & N_FLAG) == 0) {
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
			}
			cpu.pc += 2;
			break;
		// SEI
		case 0x78:
			cpu.p |= I_FLAG;
			cpu.pc += 1;
			break;
		// STX abs
		case 0x8E:
			ramWriteByte(ADDR16(cpu.pc+1), cpu.x);
			cpu.pc += 3;
			break;
		// TXS
		case 0x9A:
			cpu.sp = cpu.x;
			cpu.pc += 1;
			break;
		// LDX imm
		case 0xA2:
			cpu.x = cpuRAM[cpu.pc+1];
			cpu.pc += 2;
			if(cpu.x == 0) { cpu.p |= Z_FLAG; }
			if((cpu.x & 0x80) != 0) { cpu.p |= N_FLAG; }
			break;
		// LDA imm
		case 0xA9:
			cpu.a = cpuRAM[cpu.pc+1];
			cpu.pc += 2;
			if(cpu.a == 0) { cpu.p |= Z_FLAG; }
			if((cpu.a & 0x80) != 0) { cpu.p |= N_FLAG; }
			break;
		// LDA abs
		case 0xAD:
			cpu.a = ramReadByte(ADDR16(cpu.pc+1));
			cpu.pc += 3;
			if(cpu.a == 0) { cpu.p |= Z_FLAG; }
			if((cpu.a & 0x80) != 0) { cpu.p |= N_FLAG; }
			break;
		// CMP zp
		case 0xC5:
			{
				uint8_t tmp = cpuRAM[cpu.pc+1];
				if(cpu.a > tmp) { cpu.p |= C_FLAG; }
				if(cpu.a == tmp) { cpu.p |= Z_FLAG; }
				if(tmp & 0x80) { cpu.p |= N_FLAG; }
			}
			cpu.pc += 2;
			
			break;
		// CLD
		case 0xD8:
			cpu.p &= ~(D_FLAG);
			cpu.pc += 1;
			break;
		default:
			printf("unimplemented opcode %02X\n", opcode);
			exit(1);
	}

	return 0;
}
