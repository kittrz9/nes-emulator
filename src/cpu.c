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

	#ifdef DEBUG
		printf("pc: %04X\n", cpu.pc);
		printf("opcode: %02X\n", opcode);
		printf("cycles: %u\n", cpu.cycles);
		printf("a: %02X, x: %02X, y: %02X\n", cpu.a, cpu.x, cpu.y);
		printf("p: %02X\n", cpu.p);
		printf("s: %02X\n", cpu.s);
		printf("\n");
	#endif


	// https://www.masswerk.at/6502/6502_instruction_set.html
	// https://www.nesdev.org/obelisk-6502-guide/reference.html
	switch(opcode) {
		// ORA zp
		case 0x05:
			cpu.a |= ramReadByte(cpuRAM[cpu.pc+1]);
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// ASL zp
		case 0x06:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				set_flag(C_FLAG, (tmp & 0x80) != 0);
				tmp <<= 1;
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, ((tmp & 0x80) != 0));
				ramWriteByte(cpuRAM[cpu.pc+1], tmp);
				cpu.pc += 2;
				cpu.cycles += 5;
				break;
			}
		// ORA imm
		case 0x09:
			cpu.a |= cpuRAM[cpu.pc+1];
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// ASL A
		case 0x0A:
			set_flag(C_FLAG, (cpu.a & 0x80) != 0);
			cpu.a <<= 1;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80));
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// ASL abs
		case 0x0E:
			{
				uint8_t tmp = ramReadByte(ADDR16(cpuRAM[cpu.pc+1]));
				set_flag(C_FLAG, (tmp & 0x80) != 0);
				tmp <<= 1;
				ramWriteByte(ADDR16(cpuRAM[cpu.pc+1]), tmp);
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, (tmp & 0x80));
				cpu.pc += 3;
				cpu.cycles += 6;
			}
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
			cpu.pc += 2;
			push((cpu.pc & 0xFF00) >> 8);
			push(cpu.pc & 0xFF);
			cpu.pc -= 2;
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
		// AND zp
		case 0x25:
			cpu.a &= ramReadByte(cpuRAM[cpu.pc+1]);
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// ROL zp
		case 0x26:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				uint8_t carry = cpu.p & C_FLAG;
				set_flag(C_FLAG, tmp & 0x80);
				tmp <<= 1;
				tmp |= carry;
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, (tmp & 0x80) != 0);
				ramWriteByte(cpuRAM[cpu.pc+1], tmp);
				cpu.pc += 2;
				cpu.cycles += 5;
			}
			break;
		// AND imm
		case 0x29:
			cpu.a &= cpuRAM[cpu.pc+1];
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// ROL A
		case 0x2A:
			{
				uint8_t carry = cpu.p & C_FLAG;
				set_flag(C_FLAG, cpu.a & 0x80);
				cpu.a <<= 1;
				cpu.a |= carry;
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
				cpu.pc += 1;
				cpu.cycles += 2;
			}
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
		// AND abs
		case 0x2D:
			cpu.a &= ramReadByte(ADDR16(cpu.pc+1));
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// ROL abs
		case 0x2E:
			{
				uint8_t tmp = ramReadByte(ADDR16(cpu.pc+1));
				uint8_t carry = cpu.p & C_FLAG;
				set_flag(C_FLAG, tmp & 0x80);
				tmp >>= 1;
				tmp |= carry << 7;
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, (tmp & 0x80) != 0);
				ramWriteByte(ADDR16(cpu.pc+1), tmp);
				cpu.pc += 3;
				cpu.cycles += 7;
			}
			break;
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
		// AND abs, X
		case 0x3D:
			cpu.a &= ADDR16(cpu.pc+1) + cpu.x;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 3;
			cpu.cycles += 4;
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
			cpu.a = cpu.a ^ ramReadByte(cpuRAM[cpu.pc+1]);
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// LSR zp
		case 0x46:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				set_flag(C_FLAG, tmp & 0x01);
				tmp >>= 1;
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, ((tmp & 0x80) != 0));
				ramWriteByte(cpuRAM[cpu.pc+1], tmp);
				cpu.pc += 2;
				cpu.cycles += 5;
				break;
			}
			break;
		// PHA
		case 0x48:
			push(cpu.a);
			cpu.pc += 1;
			cpu.cycles += 3;
			break;
		// EOR imm
		case 0x49:
			cpu.a = cpu.a ^ cpuRAM[cpu.pc+1];
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 2;
			cpu.cycles += 2;
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
			cpu.pc = pop();
			cpu.pc |= pop()<<8;
			++cpu.pc;
			cpu.cycles += 6;
			break;
		// ADC zp
		case 0x65:
			{
				uint8_t byte = ramReadByte(cpuRAM[cpu.pc+1]);
				uint16_t tmp = cpu.a + byte + (cpu.p & C_FLAG);
				set_flag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
				cpu.a = tmp & 0xFF;
				set_flag(C_FLAG, tmp > 255);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			}
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// ROR zp
		case 0x66:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				uint8_t carry = cpu.p & C_FLAG;
				set_flag(C_FLAG, tmp & 0x01);
				tmp >>= 1;
				tmp |= carry << 7;
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, (tmp & 0x80) != 0);
				ramWriteByte(cpuRAM[cpu.pc+1], tmp);
				cpu.pc += 2;
				cpu.cycles += 5;
			}

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
				uint8_t byte = cpuRAM[cpu.pc+1];
				uint16_t tmp = cpu.a + byte + (cpu.p & C_FLAG);
				set_flag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
				cpu.a = tmp & 0xFF;
				set_flag(C_FLAG, tmp > 255);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			}
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// ROR A
		case 0x6A:
			{
				uint8_t carry = cpu.p & C_FLAG;
				set_flag(C_FLAG, cpu.a & 0x01);
				cpu.a >>= 1;
				cpu.a |= carry << 7;
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
				cpu.pc += 1;
				cpu.cycles += 2;
			}
			break;
		// JMP (ind)
		case 0x6C:
			cpu.pc = ADDR16(ADDR16(cpu.pc+1));
			cpu.cycles += 5;
			break;
		// ADC abs
		case 0x6D:
			{
				uint8_t byte = ramReadByte(ADDR16(cpuRAM[cpu.pc+1]));
				uint16_t tmp = cpu.a + byte + (cpu.p & C_FLAG);
				set_flag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
				cpu.a = tmp & 0xFF;
				set_flag(C_FLAG, tmp > 255);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			}
			cpu.pc += 3;
			cpu.cycles += 3;
			break;
		// ADC (ind), Y
		case 0x71:
			{
				uint8_t byte = ramReadByte(ADDR16(cpuRAM[cpu.pc+1]) + cpu.y);
				uint16_t tmp = cpu.a + byte + (cpu.p & C_FLAG);
				set_flag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
				cpu.a = tmp & 0xFF;
				set_flag(C_FLAG, tmp > 255);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			}
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// SEI
		case 0x78:
			cpu.p |= I_FLAG;
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// ADC abs, Y
		case 0x79:
			{
				uint8_t byte = ramReadByte(ADDR16(cpu.pc+1) + cpu.y);
				uint16_t tmp = cpu.a + byte + (cpu.p & C_FLAG);
				set_flag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
				cpu.a = tmp & 0xFF;
				set_flag(C_FLAG, tmp > 255);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			}
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// ROR abs, X
		case 0x7E:
			{
				uint8_t tmp = ramReadByte(ADDR16(cpu.pc+1) + cpu.x);
				uint8_t carry = cpu.p & C_FLAG;
				set_flag(C_FLAG, tmp & 0x80);
				tmp >>= 1;
				tmp |= carry << 7;
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, (tmp & 0x80) != 0);
				ramWriteByte(ADDR16(cpu.pc+1), tmp);
				cpu.pc += 3;
				cpu.cycles += 7;
			}
			break;
			break;
		// STY zp
		case 0x84:
			ramWriteByte(cpuRAM[cpu.pc+1], cpu.y);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// STA zp
		case 0x85:
			ramWriteByte(cpuRAM[cpu.pc+1], cpu.a);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// STX zp
		case 0x86:
			ramWriteByte(cpuRAM[cpu.pc+1], cpu.x);
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
		// STY zp, X
		case 0x94:
			ramWriteByte((cpuRAM[cpu.pc+1] + cpu.x) & 0xFF, cpu.y);
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// STA zp, X
		case 0x95:
			ramWriteByte((cpuRAM[cpu.pc+1] + cpu.x) & 0xFF, cpu.a);
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
		// STA abs, Y
		case 0x99:
			ramWriteByte(ADDR16(cpu.pc+1)+cpu.y, cpu.a);
			cpu.pc += 3;
			cpu.cycles += 5;
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
		// LDY zp
		case 0xA4:
			cpu.y = ramReadByte(cpuRAM[cpu.pc+1]);
			cpu.pc += 2;
			set_flag(Z_FLAG, cpu.y == 0);
			set_flag(N_FLAG, (cpu.y & 0x80) != 0);
			cpu.cycles += 3;
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
			cpu.x = ramReadByte(cpuRAM[cpu.pc+1]);
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
		// BCS
		case 0xB0:
			if((cpu.p & C_FLAG) != 0) {
				uint8_t oldPage = cpu.pc >> 8;
				cpu.pc += (int8_t)cpuRAM[cpu.pc+1];
				if(cpu.pc>>8 != oldPage) { cpu.cycles += 1; }
				cpu.cycles += 1;
			}
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
			break;
		// LDA (ind), Y
		case 0xB1:
			cpu.a = ramReadByte(ADDR16(cpuRAM[cpu.pc+1])+cpu.y);
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.cycles += 5; // figure out what the docs mean when they say to add 1 cycle when it "crosses a page boundary" here
			cpu.pc += 2;
			break;
		// LDY zp, X
		case 0xB4:
			cpu.y = ramReadByte(cpuRAM[cpu.pc+1]+ cpu.x);
			cpu.pc += 2;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.cycles += 4;
			break;
		// LDA zp, X
		case 0xB5:
			cpu.a = ramReadByte(cpuRAM[cpu.pc+1]+ cpu.x);
			cpu.pc += 2;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.cycles += 4;
			break;
		// LDA abs, Y
		case 0xB9:
			cpu.a = ramReadByte(ADDR16(cpu.pc+1)+ cpu.y);
			cpu.pc += 3;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.cycles += 4;
			break;
		// TSX
		case 0xBA:
			cpu.x = cpu.s;
			set_flag(Z_FLAG, cpu.x == 0);
			set_flag(N_FLAG, (cpu.x & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// LDY abs, X
		case 0xBC:
			cpu.y = ramReadByte(ADDR16(cpu.pc+1)+ cpu.x);
			cpu.pc += 3;
			set_flag(Z_FLAG, cpu.a == 0);
			set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.cycles += 4;
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
		// LDX abs, Y
		case 0xBE:
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
		// CPY zp
		case 0xC4:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				set_flag(C_FLAG, cpu.y >= tmp);
				set_flag(Z_FLAG, cpu.y == tmp);
				set_flag(N_FLAG, (cpu.y - tmp) & 0x80);
			}
			cpu.pc += 2;
			cpu.cycles += 3;
			
			break;
		// CMP zp
		case 0xC5:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				set_flag(C_FLAG, cpu.a >= tmp);
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
				set_flag(C_FLAG, cpu.a >= tmp);
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
		// CMP abs
		case 0xCD:
			{
				uint8_t tmp = ramReadByte(ADDR16(cpuRAM[cpu.pc+1]));
				set_flag(C_FLAG, cpu.a >= tmp);
				set_flag(Z_FLAG, cpu.a == tmp);
				set_flag(N_FLAG, tmp & 0x80);
			}
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// DEC abs
		case 0xCE:
			{
				uint8_t tmp = ramReadByte(ADDR16(cpu.pc+1));
				--tmp;
				ramWriteByte(ADDR16(cpu.pc+1), tmp);
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, (tmp & 0x80) != 0);
			}
			cpu.pc += 3;
			cpu.cycles += 6;

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
		// CMP (ind), Y
		case 0xD1:
			{
				uint8_t tmp = ramReadByte(ADDR16(cpuRAM[cpu.pc+1]) + cpu.y);
				set_flag(C_FLAG, cpu.a >= tmp);
				set_flag(Z_FLAG, cpu.a == tmp);
				set_flag(N_FLAG, tmp & 0x80);
			}
			cpu.pc += 2;
			cpu.cycles += 5;
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
		// CPX imm
		case 0xE0:
			{
				uint8_t tmp = cpuRAM[cpu.pc+1];
				set_flag(C_FLAG, cpu.x >= tmp);
				set_flag(Z_FLAG, cpu.x == tmp);
				set_flag(N_FLAG, (cpu.x - tmp) & 0x80);
			}
			cpu.pc += 2;
			cpu.cycles += 3;
			
			break;
		// CPX zp
		case 0xE4:
			{
				uint8_t tmp = ramReadByte(cpuRAM[cpu.pc+1]);
				set_flag(C_FLAG, cpu.x >= tmp);
				set_flag(Z_FLAG, cpu.x == tmp);
				set_flag(N_FLAG, (cpu.x - tmp) & 0x80);
			}
			cpu.pc += 2;
			cpu.cycles += 3;
			
			break;
		// SBC zp
		case 0xE5:
			{
				uint8_t byte = ramReadByte(cpuRAM[cpu.pc+1]);
				uint16_t tmp = cpu.a - byte - (cpu.p & C_FLAG);
				set_flag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
				cpu.a = tmp & 0xFF;
				set_flag(C_FLAG, tmp > 255);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
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
		// SBC imm
		case 0xE9:
			{
				uint8_t byte = cpuRAM[cpu.pc+1];
				uint16_t tmp = cpu.a - byte - (cpu.p & C_FLAG);
				set_flag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
				cpu.a = tmp & 0xFF;
				set_flag(C_FLAG, tmp > 255);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			}
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// INC abs
		case 0xEE:
			{
				uint8_t tmp = ramReadByte(ADDR16(cpu.pc+1)) + 1;
				ramWriteByte(ADDR16(cpu.pc+1), tmp);
				set_flag(Z_FLAG, tmp == 0);
				set_flag(N_FLAG, (tmp & 0x80) != 0);
			}
			cpu.pc += 3;
			cpu.cycles += 6;
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
		// SBC abs, Y
		case 0xF9:
			{
				uint8_t byte = ramReadByte(ADDR16(cpu.pc+1) + cpu.y);
				uint16_t tmp = cpu.a - byte - (cpu.p & C_FLAG);
				set_flag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
				cpu.a = tmp & 0xFF;
				set_flag(C_FLAG, tmp > 255);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			}
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// SBC abs, X
		case 0xFD:
			{
				uint8_t byte = ramReadByte(ADDR16(cpu.pc+1) + cpu.x);
				uint16_t tmp = cpu.a - byte - (cpu.p & C_FLAG);
				set_flag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
				cpu.a = tmp & 0xFF;
				set_flag(C_FLAG, tmp > 255);
				set_flag(Z_FLAG, cpu.a == 0);
				set_flag(N_FLAG, (cpu.a & 0x80) != 0);
			}
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		default:
			printf("unimplemented opcode %02X\n", opcode);
			exit(1);
	}

	return 0;
}
