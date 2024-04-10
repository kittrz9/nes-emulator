#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>

#include "ram.h"

cpu_t cpu;

void set_flag(uint8_t flag, uint8_t value) {
	if(value) {
		cpu.p |= flag;
	} else {
		cpu.p &= ~(flag);
	}
}

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
	printf("s: %02X\n", cpu.s);
	printf("opcode: %02X\n", opcode);
	printf("cycles: %u\n", cpu.cycles);

	// https://www.masswerk.at/6502/6502_instruction_set.html
	// https://www.nesdev.org/obelisk-6502-guide/reference.html
	switch(opcode) {
		// ASL zp
		case 0x06:
			{
				uint8_t tmp = ramReadByte(cpu.pc+1);
				set_flag(C_FLAG, (tmp & 0x80) != 0);
				tmp <<= 1;
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, ((tmp & 0x80) != 0));
				ramWriteByte(cpu.pc+1, tmp);
				cpu.pc += 2;
				cpu.cycles += 5;
				break;
			}
		// ASL A
		case 0x0A:
			set_flag(C_FLAG, (cpu.a & 0x80) != 0);
			cpu.a <<= 1;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80));
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// BPL
		case 0x10:
			if((cpu.p & N_FLAG) == 0) {
				uint8_t oldPage = cpu.pc >> 8;
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
				if(cpu.pc>>8 != oldPage) { cpu.cycles += 1; }
				cpu.cycles += 1;
			}
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// CLC
		case 0x18:
			cpu.p &= ~(C_FLAG);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// JSR
		case 0x20:
			cpu.pc += 3;
			push(cpu.pc & 0xFF);
			push((cpu.pc & 0xFF00) >> 8);
			cpu.pc -= 3;
			cpu.pc = ADDR16(cpu.pc+1);
			cpu.cycles += 6;
			break;
		// BIT zp
		case 0x24:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				set_flag(Z_FLAG, (tmp & cpu.a) == 0);
				set_flag(V_FLAG, tmp & V_FLAG);
				set_flag(N_FLAG, tmp & N_FLAG);
				cpu.pc += 2;
				cpu.cycles += 3;
				break;
			}
		// ROL zp
		case 0x26:
			{
				uint8_t tmp = ramReadByte(cpu.pc+1);
				uint8_t carry = cpu.p & C_FLAG;
				set_flag(C_FLAG, tmp & 0x80);
				tmp <<= 1;
				tmp |= carry;
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, (tmp & 0x80) != 0);
				ramWriteByte(cpu.pc+1, tmp);
				cpu.pc += 2;
				cpu.cycles += 5;
				break;
			}
			break;
		// AND imm
		case 0x29:
			cpu.a &= ramReadByte(cpuRAM[cpu.pc+1]);
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// BIT abs
		case 0x2C:
			{
				uint8_t tmp = ramReadByte(ADDR16(cpu.pc+1));
				set_flag(Z_FLAG, (tmp & cpu.a) == 0);
				set_flag(V_FLAG, tmp & V_FLAG);
				set_flag(N_FLAG, tmp & N_FLAG);
				cpu.pc += 3;
				cpu.cycles += 4;
				break;
			}
		// BMI
		case 0x30:
			if((cpu.p & N_FLAG) != 0) {
				uint8_t oldPage = cpu.pc >> 8;
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
				if(cpu.pc>>8 != oldPage) { cpu.cycles += 1; }
				cpu.cycles += 1;
			}
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// SEC
		case 0x38:
			cpu.p |= C_FLAG;
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// RTI
		case 0x40:
			cpu.p = pop();
			cpu.pc = pop() << 8;
			cpu.pc |= pop();
			cpu.cycles += 6;
			break;
		// EOR zp
		case 0x45:
			cpu.a = cpu.a ^ ramReadByte(cpu.pc+1);
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// PHA
		case 0x48:
			push(cpu.a);
			cpu.pc += 1;
			cpu.cycles += 3;
			break;
		// LSR A
		case 0x4A:
			set_flag(C_FLAG, (cpu.a & 0x01) != 0);
			cpu.a >>= 1;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// JMP abs
		case 0x4C:
			cpu.pc = ADDR16(cpu.pc+1);
			cpu.cycles += 3;
			break;
		// CLI
		case 0x58:
			cpu.p &= ~(C_FLAG);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// RTS
		case 0x60:
			cpu.pc = pop()<<8;
			cpu.pc |= pop();
			cpu.cycles += 6;
			break;
		// ADC zp
		case 0x65:
			{
				uint16_t tmp = cpu.a + ramReadByte(cpu.pc+1) + (cpu.p & C_FLAG);
				cpu.a = tmp & 0xFF;
				set_flag(C_FLAG, tmp > 255);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
				set_flag(V_FLAG, cpu.a > 127);
			}
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// PLA
		case 0x68:
			cpu.a = pop();
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 4;
			break;
		// ADC imm
		case 0x69:
			{
				uint16_t tmp = cpu.a + ADDR16(cpu.pc+1) + (cpu.p & C_FLAG);
				cpu.a = tmp & 0xFF;
				set_flag(C_FLAG, tmp > 255);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
				set_flag(V_FLAG, cpu.a > 127);
			}
			cpu.pc += 2;
			break;
		// SEI
		case 0x78:
			cpu.p |= I_FLAG;
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// STA zp
		case 0x85:
			ramWriteByte(cpu.pc+1, cpu.a);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// STX zp
		case 0x86:
			ramWriteByte(cpu.pc+1, cpu.x);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// DEY
		case 0x88:
			--cpu.y;
			set_flag(Z_FLAG, cpu.y == 0);
			set_flag(N_FLAG, (cpu.y & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// TXA
		case 0x8A:
			cpu.a = cpu.x;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// STY abs
		case 0x8C:
			ramWriteByte(ADDR16(cpu.pc+1), cpu.y);
			cpu.pc += 3;
			cpu.cycles += 3;
			break;
		// STA abs
		case 0x8D:
			ramWriteByte(ADDR16(cpu.pc+1), cpu.a);
			cpu.pc += 3;
			cpu.cycles += 3;
			break;
		// STX abs
		case 0x8E:
			ramWriteByte(ADDR16(cpu.pc+1), cpu.x);
			cpu.pc += 3;
			cpu.cycles += 3;
			break;
		// BCC
		case 0x90:
			if((cpu.p & C_FLAG) == 0) {
				uint8_t oldPage = cpu.pc >> 8;
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
				if(cpu.pc>>8 != oldPage) { cpu.cycles += 1; }
				cpu.cycles += 1;
			}
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// STA (ind), Y
		case 0x91:
			ramWriteByte(ADDR16(cpuRAM[cpu.pc+1])+cpu.y, cpu.a);
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// STA zp, X
		case 0x95:
			ramWriteByte(cpuRAM[cpu.pc+1] + cpu.x, cpu.a);
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// TYA
		case 0x98:
			cpu.a = cpu.y;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// TXS
		case 0x9A:
			cpu.s = cpu.x;
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// STA abs, X
		case 0x9D:
			ramWriteByte(ADDR16(cpu.pc+1)+cpu.x, cpu.a);
			cpu.pc += 3;
			cpu.cycles += 5;
			break;
		// LDY imm
		case 0xA0:
			cpu.y = cpuRAM[cpu.pc+1];
			cpu.pc += 2;
			set_flag(Z_FLAG, cpu.y == 0);
			set_flag(N_FLAG, (cpu.y & 0x80) != 0);
			cpu.cycles += 2;
			break;
		// LDX imm
		case 0xA2:
			cpu.x = cpuRAM[cpu.pc+1];
			cpu.pc += 2;
			set_flag(Z_FLAG, cpu.x == 0);
			set_flag(N_FLAG, (cpu.x & 0x80) != 0);
			cpu.cycles += 2;
			break;
		// LDA zp
		case 0xA5:
			cpu.a = ramReadByte(cpuRAM[cpu.pc+1]);
			cpu.pc += 2;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.cycles += 3;
			break;
		// LDX zp
		case 0xA6:
			cpu.a = ramReadByte(cpuRAM[cpu.pc+1]);
			cpu.pc += 2;
			set_flag(Z_FLAG, cpu.x == 0);
			set_flag(N_FLAG, (cpu.x & 0x80) != 0);
			cpu.cycles += 3;
			break;
		// TAY
		case 0xA8:
			cpu.y = cpu.a;
			set_flag(Z_FLAG, cpu.y == 0);
			set_flag(N_FLAG, (cpu.y & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// LDA imm
		case 0xA9:
			cpu.a = cpuRAM[cpu.pc+1];
			cpu.pc += 2;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.cycles += 2;
			break;
		// TAX
		case 0xAA:
			cpu.x = cpu.a;
			set_flag(Z_FLAG, cpu.x == 0);
			set_flag(N_FLAG, (cpu.x & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// LDY abs
		case 0xAC:
			cpu.y = ramReadByte(ADDR16(cpu.pc+1));
			cpu.pc += 3;
			set_flag(Z_FLAG, cpu.y == 0);
			set_flag(N_FLAG, (cpu.y & 0x80) != 0);
			cpu.cycles += 4;
			break;
		// LDA abs
		case 0xAD:
			cpu.a = ramReadByte(ADDR16(cpu.pc+1));
			cpu.pc += 3;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.cycles += 4;
			break;
		// LDX abs
		case 0xAE:
			cpu.x = ramReadByte(ADDR16(cpu.pc+1));
			cpu.pc += 3;
			set_flag(Z_FLAG, cpu.x == 0);
			set_flag(N_FLAG, (cpu.x & 0x80) != 0);
			cpu.cycles += 4;
			break;
		// LDA abs, Y
		case 0xB9:
			{
				uint16_t tmp = ADDR16(cpu.pc+1);
				cpu.a = ramReadByte(tmp + cpu.y);
				cpu.pc += 3;
				if(tmp >> 8 != (tmp + cpu.y) >> 8) { cpu.cycles += 1; }
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
				cpu.cycles += 4;
			}
			break;
		// LDA abs, X
		case 0xBD:
			{
				uint16_t tmp = ADDR16(cpu.pc+1);
				cpu.a = ramReadByte(tmp + cpu.x);
				cpu.pc += 3;
				if(tmp >> 8 != (tmp + cpu.x) >> 8) { cpu.cycles += 1; }
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
				cpu.cycles += 4;
			}
			break;
		// CPY imm
		case 0xC0:
			{
				uint8_t tmp = cpuRAM[cpu.pc+1];
				set_flag(C_FLAG, cpu.y > tmp);
				set_flag(Z_FLAG, cpu.y == tmp);
				set_flag(N_FLAG, tmp & 0x80);
			}
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// CMP zp
		case 0xC5:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				set_flag(C_FLAG, cpu.a > tmp);
				set_flag(Z_FLAG, cpu.a == tmp);
				set_flag(N_FLAG, tmp & 0x80);
			}
			cpu.pc += 2;
			cpu.cycles += 3;
			
			break;
		// DEC zp
		case 0xC6:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				--tmp;
				ramWriteByte(cpuRAM[cpu.pc+1], tmp);
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, (tmp & 0x80) != 0);
			}
			cpu.pc += 2;
			cpu.cycles += 5;

			break;
		// INY
		case 0xC8:
			++cpu.y;
			set_flag(Z_FLAG, cpu.y == 0);
			set_flag(N_FLAG, (cpu.y & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// CMP imm
		case 0xC9:
			{
				uint8_t tmp = cpuRAM[cpu.pc+1];
				set_flag(C_FLAG, cpu.a > tmp);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, tmp & 0x80);
			}
			cpu.pc += 2;
			cpu.cycles += 2;
			
			break;
		// DEX
		case 0xCA:
			--cpu.x;
			set_flag(Z_FLAG, cpu.x == 0);
			set_flag(N_FLAG, (cpu.x & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// BNE
		case 0xD0:
			if((cpu.p & Z_FLAG) == 0) {
				uint8_t oldPage = cpu.pc >> 8;
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
				if(cpu.pc>>8 != oldPage) { cpu.cycles += 1; }
				cpu.cycles += 1;
			}
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// CLD
		case 0xD8:
			cpu.p &= ~(D_FLAG);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// DEC abs, X
		case 0xDE:
			{
				uint8_t tmp = ramReadByte(ADDR16(cpu.pc+1));
				--tmp;
				ramWriteByte(cpuRAM[cpu.pc+1], tmp);
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, (tmp & 0x80) != 0);
			}
			cpu.pc += 3;
			cpu.cycles += 7;

			break;
		// CPX zp
		case 0xE0:
			{
				uint8_t tmp = cpuRAM[cpu.pc+1];
				set_flag(C_FLAG, cpu.x > tmp);
				set_flag(Z_FLAG, cpu.x == tmp);
				set_flag(N_FLAG, tmp & 0x80);
			}
			cpu.pc += 2;
			cpu.cycles += 3;
			
			break;
		// INC zp
		case 0xE6:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				++tmp;
				ramWriteByte(cpuRAM[cpu.pc+1], tmp);
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, (tmp & 0x80) != 0);
			}
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// INX
		case 0xE8:
			++cpu.x;
			set_flag(Z_FLAG, cpu.x == 0);
			set_flag(N_FLAG, (cpu.x & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// BEQ
		case 0xF0:
			if((cpu.p & Z_FLAG) != 0) {
				uint8_t oldPage = cpu.pc >> 8;
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
				if(cpu.pc>>8 != oldPage) { cpu.cycles += 1; }
				cpu.cycles += 1;
			}
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		default:
			printf("unimplemented opcode %02X\n", opcode);
			exit(1);
	}

	return 0;
}
