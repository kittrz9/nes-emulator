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
	cpu.s = 0xFD;
	return;
}

void push(uint8_t byte) {
	ramWriteByte(0x100 + cpu.s--, byte);
	return;
}

uint8_t pop() {
	return ramReadByte(0x100 + ++cpu.s);
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
		// ASL zp
		case 0x06:
			{
				uint8_t tmp = ramReadByte(cpu.pc+1);
				if((tmp & 0x80) != 0) { cpu.p |= C_FLAG; }
				tmp <<= 1;
				if(tmp == 0) { cpu.p |= Z_FLAG; }
				if((tmp & 0x80) != 0) { cpu.p |= N_FLAG; }
				ramWriteByte(cpu.pc+1, tmp);
				cpu.pc += 2;
				break;
			}
		// ASL A
		case 0x0A:
			if((cpu.a & 0x80) != 0) { cpu.p |= C_FLAG; }
			cpu.a <<= 1;
			if(cpu.a == 0) { cpu.p |= Z_FLAG; }
			if((cpu.a & 0x80) != 0) { cpu.p |= N_FLAG; }
			cpu.pc += 1;
			break;
		// BPL
		case 0x10:
			if((cpu.p & N_FLAG) == 0) {
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
			}
			cpu.pc += 2;
			break;
		// CLC
		case 0x18:
			cpu.p &= ~(C_FLAG);
			cpu.pc += 1;
			break;
		// JSR
		case 0x20:
			cpu.pc += 3;
			push(cpu.pc & 0xFF);
			push((cpu.pc & 0xFF00) >> 8);
			cpu.pc -= 3;
			cpu.pc = ADDR16(cpu.pc+1);
			break;
		// BIT zp
		case 0x24:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				if((tmp & cpu.a) == 0) { cpu.p |= Z_FLAG; }
				cpu.p |= tmp & 0xC0;
				cpu.pc += 2;
				break;
			}
		// AND imm
		case 0x29:
			cpu.a &= ramReadByte(cpuRAM[cpu.pc+1]);
			if(cpu.a == 0) { cpu.p |= Z_FLAG; }
			if((cpu.a & 0x80) != 0) { cpu.p |= N_FLAG; }
			cpu.pc += 2;
			break;
		// BMI
		case 0x30:
			if((cpu.p & N_FLAG) != 0) {
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
			}
			cpu.pc += 2;
			break;
		// SEC
		case 0x38:
			cpu.p |= C_FLAG;
			cpu.pc += 1;
			break;
		// EOR zp
		case 0x45:
			cpu.a = cpu.a ^ ramReadByte(cpu.pc+1);
			if(cpu.a == 0) { cpu.p |= Z_FLAG; }
			if((cpu.a & 0x80) != 0) { cpu.p |= N_FLAG; }
			cpu.pc += 2;
			break;
		// JMP abs
		case 0x4C:
			cpu.pc = ADDR16(cpu.pc+1);
			break;
		// RTS
		case 0x60:
			cpu.pc = pop()<<8;
			cpu.pc |= pop();
			break;
		// ADC zp
		case 0x65:
			{
				uint16_t tmp = cpu.a + ramReadByte(cpu.pc+1) + (cpu.p & C_FLAG);
				cpu.a = tmp & 0xFF;
				if(tmp > 255) { cpu.p |= C_FLAG; }
				if(cpu.a == 0) { cpu.p |= Z_FLAG; }
				if((cpu.a & 0x80) != 0) { cpu.p |= N_FLAG; }
				if(cpu.a > 127) { cpu.p |= V_FLAG; }
			}
			cpu.pc += 2;
			break;
		// SEI
		case 0x78:
			cpu.p |= I_FLAG;
			cpu.pc += 1;
			break;
		// STA zp
		case 0x85:
			ramWriteByte(cpu.pc+1, cpu.a);
			cpu.pc += 2;
			break;
		// STX zp
		case 0x86:
			ramWriteByte(cpu.pc+1, cpu.x);
			cpu.pc += 2;
			break;
		// DEY
		case 0x88:
			--cpu.y;
			if(cpu.y == 0) { cpu.p |= Z_FLAG; }
			if((cpu.y & 0x80) != 0) { cpu.p |= N_FLAG; }
			cpu.pc += 1;
			break;
		// STA abs
		case 0x8D:
			ramWriteByte(ADDR16(cpu.pc+1), cpu.a);
			cpu.pc += 3;
			break;
		// STX abs
		case 0x8E:
			ramWriteByte(ADDR16(cpu.pc+1), cpu.x);
			cpu.pc += 3;
			break;
		// BCC
		case 0x90:
			if((cpu.p & C_FLAG) == 0) {
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
			}
			cpu.pc += 2;
			break;
		// STA ind, Y
		case 0x91:
			ramWriteByte(cpuRAM[cpu.pc+1], cpu.a);
			cpu.a += cpu.y;
			cpu.pc += 2;
			break;
		// TXS
		case 0x9A:
			cpu.s = cpu.x;
			cpu.pc += 1;
			break;
		// STA abs, X
		case 0x9D:
			ramWriteByte(ADDR16(cpu.pc+1)+cpu.x, cpu.a);
			cpu.pc += 3;
			break;
		// LDY imm
		case 0xA0:
			cpu.y = cpuRAM[cpu.pc+1];
			cpu.pc += 2;
			if(cpu.y == 0) { cpu.p |= Z_FLAG; }
			if((cpu.y & 0x80) != 0) { cpu.p |= N_FLAG; }
			break;
		// LDX imm
		case 0xA2:
			cpu.x = cpuRAM[cpu.pc+1];
			cpu.pc += 2;
			if(cpu.x == 0) { cpu.p |= Z_FLAG; }
			if((cpu.x & 0x80) != 0) { cpu.p |= N_FLAG; }
			break;
		// LDA zp
		case 0xA5:
			cpu.a = ramReadByte(cpuRAM[cpu.pc+1]);
			cpu.pc += 2;
			if(cpu.a == 0) { cpu.p |= Z_FLAG; }
			if((cpu.a & 0x80) != 0) { cpu.p |= N_FLAG; }
			break;
		// LDX zp
		case 0xA6:
			cpu.a = ramReadByte(cpuRAM[cpu.pc+1]);
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
		// LDA abs, X
		case 0xBD:
			cpu.a = ramReadByte(ADDR16(cpu.pc+1) + cpu.x);
			cpu.pc += 3;
			if(cpu.a == 0) { cpu.p |= Z_FLAG; }
			if((cpu.a & 0x80) != 0) { cpu.p |= N_FLAG; }
			break;
		// CMP zp
		case 0xC5:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				if(cpu.a > tmp) { cpu.p |= C_FLAG; }
				if(cpu.a == tmp) { cpu.p |= Z_FLAG; }
				if(tmp & 0x80) { cpu.p |= N_FLAG; }
			}
			cpu.pc += 2;
			
			break;
		// DEC zp
		case 0xC6:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				--tmp;
				ramWriteByte(cpuRAM[cpu.pc+1], tmp);
				if(tmp == 0) { cpu.p |= Z_FLAG; }
				if((tmp & 0x80) != 0) { cpu.p |= N_FLAG; }
			}
			cpu.pc += 2;

			break;
		// CMP imm
		case 0xC9:
			{
				uint8_t tmp = cpuRAM[cpu.pc+1];
				if(cpu.a > tmp) { cpu.p |= C_FLAG; }
				if(cpu.a == tmp) { cpu.p |= Z_FLAG; }
				if(tmp & 0x80) { cpu.p |= N_FLAG; }
			}
			cpu.pc += 2;
			
			break;
		// DEX
		case 0xCA:
			--cpu.x;
			if(cpu.x == 0) { cpu.p |= Z_FLAG; }
			if((cpu.x & 0x80) != 0) { cpu.p |= N_FLAG; }
			cpu.pc += 1;
			break;
		// BNE
		case 0xD0:
			if((cpu.p & Z_FLAG) == 0) {
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
			}
			cpu.pc += 2;
			break;
		// CLD
		case 0xD8:
			cpu.p &= ~(D_FLAG);
			cpu.pc += 1;
			break;
		// DEC abs, X
		case 0xDE:
			{
				uint8_t tmp = ramReadByte(ADDR16(cpu.pc+1));
				--tmp;
				ramWriteByte(cpuRAM[cpu.pc+1], tmp);
				if(tmp == 0) { cpu.p |= Z_FLAG; }
				if((tmp & 0x80) != 0) { cpu.p |= N_FLAG; }
			}
			cpu.pc += 3;

			break;
		// CPX zp
		case 0xE0:
			{
				uint8_t tmp = cpuRAM[cpu.pc+1];
				if(cpu.x > tmp) { cpu.p |= C_FLAG; }
				if(cpu.x == tmp) { cpu.p |= Z_FLAG; }
				if(tmp & 0x80) { cpu.p |= N_FLAG; }
			}
			cpu.pc += 2;
			
			break;
		// INC zp
		case 0xE6:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				++tmp;
				ramWriteByte(cpuRAM[cpu.pc+1], tmp);
				if(tmp == 0) { cpu.p |= Z_FLAG; }
				if((tmp & 0x80) != 0) { cpu.p |= N_FLAG; }
			}
			cpu.pc += 2;
			break;
		// INX
		case 0xE8:
			++cpu.x;
			if(cpu.x == 0) { cpu.p |= Z_FLAG; }
			if((cpu.x & 0x80) != 0) { cpu.p |= N_FLAG; }
			cpu.pc += 1;
			break;
		// BEQ
		case 0xF0:
			if((cpu.p & Z_FLAG) != 0) {
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
			}
			cpu.pc += 2;
			break;
		default:
			printf("unimplemented opcode %02X\n", opcode);
			exit(1);
	}

	return 0;
}
