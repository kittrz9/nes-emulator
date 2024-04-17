#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>

#include "ram.h"

cpu_t cpu;

#define ARG8 ramReadByte(cpu.pc+1)
#define ARG16 ADDR16(cpu.pc+1)
#define IMM ARG8
#define ZP ramReadByte(ARG8)
#define ABS ramReadByte(ARG16)
#define ZP_INDEX(v) ramReadByte((ARG8 + v) & 0xFF)
#define ABS_INDEX(v) ramReadByte(ARG16 + v)
#define INDIR_INDEX_Y ramReadByte(ADDR16(ARG8)+cpu.y)
#define INDEX_INDIR_X ramReadByte(ADDR16(ARG8+cpu.x))

#define ZP_ADDR ARG8
#define ABS_ADDR ARG16
#define ZP_INDEX_ADDR(v) ((ARG8 + v) & 0xFF)
#define ABS_INDEX_ADDR(v) (ARG16 + v)
#define INDIR_INDEX_Y_ADDR (ADDR16(ARG8)+cpu.y)
#define INDEX_INDIR_X_ADDR (ADDR16(ARG8+cpu.x))

void cpuDumpState(void) {
	printf("pc: %04X\n", cpu.pc);
	printf("opcode: %02X\n", ramReadByte(cpu.pc));
	printf("cycles: %u\n", cpu.cycles);
	printf("a: %02X, x: %02X, y: %02X\n", cpu.a, cpu.x, cpu.y);
	printf("p: %02X\n", cpu.p);
	printf("s: %02X\n", cpu.s);
	printf("\n");
}

void set_flag(uint8_t flag, uint8_t value) {
	if(value) {
		cpu.p |= flag;
	} else {
		cpu.p &= ~(flag);
	}
}

void cpuInit(void) {
	cpu.pc = ADDR16(RST_VECTOR);
	cpu.s = 0xFD;
	return;
}

void push(uint8_t byte) {
	ramWriteByte(0x100 + cpu.s--, byte);
	return;
}

uint8_t pop(void) {
	return ramReadByte(0x100 + ++cpu.s);
}

void branch(uint8_t cond) {
	if(cond) {
		uint8_t oldPage = cpu.pc >> 8;
		cpu.pc += (int8_t)IMM;
		if(cpu.pc>>8 != oldPage) { cpu.cycles += 1; }
		cpu.cycles += 1;
	}
	return;
}

void cmp(uint8_t reg, uint8_t byte) {
	set_flag(C_FLAG, reg >= byte);
	set_flag(Z_FLAG, reg == byte);
	set_flag(N_FLAG, (reg - byte) & 0x80);
	return;
}

void bit(uint8_t byte) {
	set_flag(Z_FLAG, (byte & cpu.a) == 0);
	set_flag(V_FLAG, byte & V_FLAG);
	set_flag(N_FLAG, byte & N_FLAG);
	return;
}

void ora(uint8_t byte) {
	cpu.a |= byte;
	set_flag(Z_FLAG, cpu.a == 0);
	set_flag(N_FLAG, (cpu.a & 0x80) != 0);
	return;
}

void and_a(uint8_t byte) {
	cpu.a &= byte;
	set_flag(Z_FLAG, cpu.a == 0);
	set_flag(N_FLAG, (cpu.a & 0x80) != 0);
	return;
}

void eor(uint8_t byte) {
	cpu.a = cpu.a ^ byte;
	set_flag(Z_FLAG, cpu.a == 0);
	set_flag(N_FLAG, (cpu.a & 0x80) != 0);
}

// return result so it can be put into ram or A
uint8_t asl(uint8_t byte) {
	set_flag(C_FLAG, byte & 0x80);
	byte <<= 1;
	set_flag(Z_FLAG, byte == 0);
	set_flag(N_FLAG, byte & 0x80);
	return byte;
}

uint8_t lsr(uint8_t byte) {
	set_flag(C_FLAG, byte & 0x01);
	byte >>= 1;
	set_flag(Z_FLAG, byte == 0);
	set_flag(N_FLAG, byte & 0x80);
	return byte;
}

uint8_t ror(uint8_t byte) {
	uint8_t carry = cpu.p & C_FLAG;
	set_flag(C_FLAG, byte & 0x01);
	byte >>= 1;
	byte |= carry << 7;
	set_flag(Z_FLAG, byte == 0);
	set_flag(N_FLAG, (byte & 0x80) != 0);
	return byte;
}

uint8_t rol(uint8_t byte) {
	uint8_t carry = cpu.p & C_FLAG;
	set_flag(C_FLAG, byte & 0x80);
	byte <<= 1;
	byte |= carry;
	set_flag(Z_FLAG, byte == 0);
	set_flag(N_FLAG, (byte & 0x80) != 0);
	return byte;
}

void adc(uint8_t byte) {
	uint16_t tmp = cpu.a + byte + (cpu.p & C_FLAG);
	set_flag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
	cpu.a = tmp & 0xFF;
	set_flag(C_FLAG, tmp > 255);
	set_flag(Z_FLAG, cpu.a == 0);
	set_flag(N_FLAG, (cpu.a & 0x80) != 0);
	return;
}

void sbc(uint8_t byte) {
	uint16_t tmp = cpu.a - byte - !(cpu.p & C_FLAG);
	set_flag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
	cpu.a = tmp & 0xFF;
	set_flag(C_FLAG, tmp < 256);
	set_flag(Z_FLAG, cpu.a == 0);
	set_flag(N_FLAG, (cpu.a & 0x80) != 0);
}

uint8_t dec(uint8_t byte) {
	--byte;
	set_flag(Z_FLAG, byte == 0);
	set_flag(N_FLAG, (byte & 0x80) != 0);
	return byte;
}

uint8_t inc(uint8_t byte) {
	++byte;
	set_flag(Z_FLAG, byte == 0);
	set_flag(N_FLAG, (byte & 0x80) != 0);
	return byte;
}

// using pointers here since I don't have to worry about affecting ram
void load(uint8_t* reg, uint8_t byte) {
	*reg = byte;
	set_flag(Z_FLAG, *reg == 0);
	set_flag(N_FLAG, (*reg & 0x80) != 0);
	return;
}

void transfer(uint8_t* reg, uint8_t byte) {
	*reg = byte;
	set_flag(Z_FLAG, *reg == 0);
	set_flag(N_FLAG, (*reg & 0x80) != 0);
	return;
}

uint8_t cpuStep(void) {
	uint8_t opcode = ramReadByte(cpu.pc);

	#ifdef DEBUG
		cpuDumpState();
	#endif

	// https://www.masswerk.at/6502/6502_instruction_set.html
	// https://www.nesdev.org/obelisk-6502-guide/reference.html
	switch(opcode) {
		// ORA (ind, X)
		case 0x01:
			ora(INDEX_INDIR_X);
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// ORA zp
		case 0x05:
			ora(ZP);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// ASL zp
		case 0x06:
			ramWriteByte(ZP_ADDR, asl(ZP));
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// PHP
		case 0x08:
			push(cpu.p);
			cpu.pc += 1;
			cpu.cycles += 3;
			break;
		// ORA imm
		case 0x09:
			ora(IMM);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// ASL A
		case 0x0A:
			cpu.a = asl(cpu.a);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// ORA abs
		case 0x0D:
			ora(ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// ASL abs
		case 0x0E:
			ramWriteByte(ABS_ADDR, asl(ABS));
			cpu.pc += 3;
			cpu.cycles += 6;
			break;
		// BPL
		case 0x10:
			branch((cpu.p & N_FLAG) == 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// LDA (ind), Y
		case 0x11:
			load(&cpu.a, INDIR_INDEX_Y);
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// LDA zp, X
		case 0x15:
			load(&cpu.a, ZP_INDEX(cpu.x));
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// ASL zp, X
		case 0x16:
			ramWriteByte(ZP_INDEX_ADDR(cpu.x), asl(ZP_INDEX(cpu.x)));
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// CLC
		case 0x18:
			cpu.p &= ~(C_FLAG);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// ORA abs, Y
		case 0x19:
			ora(ABS_INDEX(cpu.y));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// ORA abs, X
		case 0x1D:
			ora(ABS_INDEX(cpu.x));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// ASL abs, X
		case 0x1E:
			ramWriteByte(ABS_INDEX_ADDR(cpu.x), asl(ABS_INDEX(cpu.x)));
			cpu.pc += 3;
			cpu.cycles += 7;
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
		// AND (ind, X)
		case 0x21:
			and_a(INDEX_INDIR_X);
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// BIT zp
		case 0x24:
			bit(ZP);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// AND zp
		case 0x25:
			and_a(ZP);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// ROL zp
		case 0x26:
			ramWriteByte(ZP_ADDR, rol(ZP));
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// PLP
		case 0x28:
			cpu.p = pop();
			cpu.pc += 1;
			cpu.cycles += 4;
			break;
		// AND imm
		case 0x29:
			and_a(IMM);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// ROL A
		case 0x2A:
			cpu.a = rol(cpu.a);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// BIT abs
		case 0x2C:
			bit(ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// AND abs
		case 0x2D:
			and_a(ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// ROL abs
		case 0x2E:
			ramWriteByte(ABS_ADDR, rol(ABS));
			cpu.pc += 3;
			cpu.cycles += 7;
			break;
		// BMI
		case 0x30:
			branch((cpu.p & N_FLAG) != 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// AND (ind), Y
		case 0x31:
			and_a(INDIR_INDEX_Y);
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// AND zp, X
		case 0x35:
			and_a(ZP_INDEX(cpu.x));
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// ROL zp, X
		case 0x36:
			ramWriteByte(ZP_INDEX_ADDR(cpu.x), rol(ZP_INDEX(cpu.x)));
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// SEC
		case 0x38:
			cpu.p |= C_FLAG;
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// AND abs, Y
		case 0x39:
			and_a(ABS_INDEX(cpu.y));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// AND abs, X
		case 0x3D:
			and_a(ABS_INDEX(cpu.x));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// ROL abs, X
		case 0x3E:
			ramWriteByte(ABS_INDEX_ADDR(cpu.x), rol(ABS_INDEX(cpu.x)));
			cpu.pc += 3;
			cpu.cycles += 7;
			break;
		// RTI
		case 0x40:
			cpu.p = pop();
			cpu.pc = pop() << 8;
			cpu.pc |= pop();
			cpu.cycles += 6;
			break;
		// EOR (ind, X)
		case 0x41:
			eor(INDEX_INDIR_X);
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// EOR zp
		case 0x45:
			eor(ZP);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// LSR zp
		case 0x46:
			ramWriteByte(ZP_ADDR, lsr(ZP));
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// PHA
		case 0x48:
			push(cpu.a);
			cpu.pc += 1;
			cpu.cycles += 3;
			break;
		// EOR imm
		case 0x49:
			eor(IMM);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// LSR A
		case 0x4A:
			cpu.a = lsr(cpu.a);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// JMP abs
		case 0x4C:
			cpu.pc = ABS_ADDR;
			cpu.cycles += 3;
			break;
		// LSR abs
		case 0x4E:
			ramWriteByte(ABS_ADDR, lsr(ABS));
			cpu.pc += 3;
			cpu.cycles += 6;
			break;
		// BVC
		case 0x50:
			branch((cpu.p & V_FLAG) == 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// EOR (ind), Y
		case 0x51:
			eor(INDIR_INDEX_Y);
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// EOR zp, X
		case 0x55:
			eor(ZP_INDEX(cpu.x));
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// LSR zp, X
		case 0x56:
			ramWriteByte(ZP_INDEX_ADDR(cpu.x), lsr(ZP_INDEX(cpu.x)));
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// EOR abs, Y
		case 0x59:
			eor(ABS_INDEX(cpu.y));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// CLI
		case 0x58:
			cpu.p &= ~(C_FLAG);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// EOR abs, X
		case 0x5D:
			eor(ABS_INDEX(cpu.x));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// LSR abs, X
		case 0x5E:
			ramWriteByte(ABS_INDEX_ADDR(cpu.x), lsr(ABS_INDEX(cpu.x)));
			cpu.pc += 3;
			cpu.cycles += 7;
			break;
		// RTS
		case 0x60:
			cpu.pc = pop();
			cpu.pc |= pop()<<8;
			++cpu.pc;
			cpu.cycles += 6;
			break;
		// ADC (ind, X)
		case 0x61:
			adc(INDEX_INDIR_X);
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// ADC zp
		case 0x65:
			adc(ZP);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// ROR zp
		case 0x66:
			ramWriteByte(ZP_ADDR, ror(ZP));
			cpu.pc += 2;
			cpu.cycles += 5;
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
			adc(IMM);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// ROR A
		case 0x6A:
			cpu.a = ror(cpu.a);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// JMP (ind)
		case 0x6C:
			cpu.pc = ADDR16(ADDR16(cpu.pc+1));
			cpu.cycles += 5;
			break;
		// ADC abs
		case 0x6D:
			adc(ABS);
			cpu.pc += 3;
			cpu.cycles += 3;
			break;
		// ROR abs
		case 0x6E:
			ramWriteByte(ABS_ADDR, ror(ABS));
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// BVS
		case 0x70:
			branch((cpu.p & V_FLAG) != 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// ADC (ind), Y
		case 0x71:
			adc(INDIR_INDEX_Y);
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// ADC zp, X
		case 0x75:
			adc(ZP_INDEX(cpu.x));
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// ROR zp, X
		case 0x76:
			ramWriteByte(ZP_ADDR, ror(ZP));
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// SEI
		case 0x78:
			cpu.p |= I_FLAG;
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// ADC abs, Y
		case 0x79:
			adc(ABS_INDEX(cpu.y));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// ADC abs, X
		case 0x7D:
			adc(ABS_INDEX(cpu.x));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// ROR abs, X
		case 0x7E:
			ramWriteByte(ABS_INDEX_ADDR(cpu.x), ror(ABS_INDEX(cpu.x)));
			cpu.pc += 3;
			cpu.cycles += 7;
			break;
		// STA (ind, X)
		case 0x81:
			cpu.a = ramReadByte(INDEX_INDIR_X_ADDR);
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// STY zp
		case 0x84:
			ramWriteByte(ZP_ADDR, cpu.y);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// STA zp
		case 0x85:
			ramWriteByte(ZP_ADDR, cpu.a);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// STX zp
		case 0x86:
			ramWriteByte(ZP_ADDR, cpu.x);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// DEY
		case 0x88:
			cpu.y = dec(cpu.y);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// TXA
		case 0x8A:
			transfer(&cpu.a, cpu.x);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// STY abs
		case 0x8C:
			ramWriteByte(ABS_ADDR, cpu.y);
			cpu.pc += 3;
			cpu.cycles += 3;
			break;
		// STA abs
		case 0x8D:
			ramWriteByte(ABS_ADDR, cpu.a);
			cpu.pc += 3;
			cpu.cycles += 3;
			break;
		// STX abs
		case 0x8E:
			ramWriteByte(ABS_ADDR, cpu.x);
			cpu.pc += 3;
			cpu.cycles += 3;
			break;
		// BCC
		case 0x90:
			branch((cpu.p & C_FLAG) == 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// STA (ind), Y
		case 0x91:
			ramWriteByte(INDIR_INDEX_Y_ADDR, cpu.a);
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// STY zp, X
		case 0x94:
			ramWriteByte(ZP_INDEX_ADDR(cpu.x), cpu.y);
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// STA zp, X
		case 0x95:
			ramWriteByte(ZP_INDEX_ADDR(cpu.x), cpu.a);
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// STX zp, Y
		case 0x96:
			ramWriteByte(ZP_INDEX_ADDR(cpu.y), cpu.x);
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// TYA
		case 0x98:
			transfer(&cpu.a, cpu.y);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// STA abs, Y
		case 0x99:
			ramWriteByte(ABS_INDEX_ADDR(cpu.y), cpu.a);
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
			ramWriteByte(ABS_INDEX_ADDR(cpu.x), cpu.a);
			cpu.pc += 3;
			cpu.cycles += 5;
			break;
		// LDY imm
		case 0xA0:
			load(&cpu.y, IMM);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// LDA (ind, X)
		case 0xA1:
			load(&cpu.a, INDEX_INDIR_X);
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// LDX imm
		case 0xA2:
			load(&cpu.x, IMM);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// LDY zp
		case 0xA4:
			load(&cpu.y, ZP);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// LDA zp
		case 0xA5:
			load(&cpu.a, ZP);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// LDX zp
		case 0xA6:
			load(&cpu.x, ZP);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// TAY
		case 0xA8:
			transfer(&cpu.y, cpu.a);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// LDA imm
		case 0xA9:
			load(&cpu.a, IMM);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// TAX
		case 0xAA:
			transfer(&cpu.x, cpu.a);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// LDY abs
		case 0xAC:
			load(&cpu.y, ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// LDA abs
		case 0xAD:
			load(&cpu.a, ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// LDX abs
		case 0xAE:
			load(&cpu.x, ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// BCS
		case 0xB0:
			branch((cpu.p & C_FLAG) != 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// LDA (ind), Y
		case 0xB1:
			load(&cpu.a, INDIR_INDEX_Y);
			cpu.pc += 2;
			cpu.cycles += 5; // figure out what the docs mean when they say to add 1 cycle when it "crosses a page boundary" here
			break;
		// LDY zp, X
		case 0xB4:
			load(&cpu.y, ZP_INDEX(cpu.x));
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// LDA zp, X
		case 0xB5:
			load(&cpu.a, ZP_INDEX(cpu.x));
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// LDX zp, Y
		case 0xB6:
			cpu.x = ramReadByte(ZP_INDEX(cpu.y));
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// CLV
		case 0xB8:
			cpu.p &= ~(V_FLAG);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// LDA abs, Y
		case 0xB9:
			load(&cpu.a, ABS_INDEX(cpu.y));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// TSX
		case 0xBA:
			transfer(&cpu.x, cpu.s);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// LDY abs, X
		case 0xBC:
			load(&cpu.y, ABS_INDEX(cpu.x));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// LDA abs, X
		case 0xBD:
			load(&cpu.a, ABS_INDEX(cpu.x));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// LDX abs, Y
		case 0xBE:
			load(&cpu.x, ABS_INDEX(cpu.y));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// CPY imm
		case 0xC0:
			cmp(cpu.y, IMM);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// CMP (ind, X)
		case 0xC1:
			cmp(cpu.a, INDEX_INDIR_X);
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// CPY zp
		case 0xC4:
			cmp(cpu.y, ZP);
			cpu.pc += 2;
			cpu.cycles += 3;
			
			break;
		// CMP zp
		case 0xC5:
			cmp(cpu.a, ZP);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// DEC zp
		case 0xC6:
			ramWriteByte(ZP_ADDR, dec(ZP));
			cpu.pc += 2;
			cpu.cycles += 5;

			break;
		// INY
		case 0xC8:
			cpu.y = inc(cpu.y);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// CMP imm
		case 0xC9:
			cmp(cpu.a, IMM);
			cpu.pc += 2;
			cpu.cycles += 2;
			
			break;
		// DEX
		case 0xCA:
			cpu.x = dec(cpu.x);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// CPY abs
		case 0xCC:
			cmp(cpu.y, ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// CMP abs
		case 0xCD:
			cmp(cpu.a, ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// DEC abs
		case 0xCE:
			ramWriteByte(ABS_ADDR, dec(ABS));
			cpu.pc += 3;
			cpu.cycles += 6;

			break;
		// BNE
		case 0xD0:
			branch((cpu.p & Z_FLAG) == 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// CMP (ind), Y
		case 0xD1:
			cmp(cpu.a, INDIR_INDEX_Y);
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// CMP zp, X
		case 0xD5:
			cmp(cpu.a, ZP_INDEX(cpu.x));
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// DEC zp, X
		case 0xD6:
			ramWriteByte(ZP_INDEX_ADDR(cpu.x), dec(ZP_INDEX(cpu.x)));
			cpu.pc += 2;
			cpu.cycles += 7;
			break;
		// CLD
		case 0xD8:
			cpu.p &= ~(D_FLAG);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// CMP abs, Y
		case 0xD9:
			cmp(cpu.a, ABS_INDEX(cpu.y));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// CMP abs, X
		case 0xDD:
			cmp(cpu.a, ABS_INDEX(cpu.x));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// DEC abs, X
		case 0xDE:
			ramWriteByte(ABS_INDEX_ADDR(cpu.x), dec(ABS_INDEX(cpu.x)));
			cpu.pc += 3;
			cpu.cycles += 7;

			break;
		// CPX imm
		case 0xE0:
			cmp(cpu.x, IMM);
			cpu.pc += 2;
			cpu.cycles += 3;
			
			break;
		// SBC (ind, X)
		case 0xE1:
			sbc(INDEX_INDIR_X);
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// CPX zp
		case 0xE4:
			cmp(cpu.x, ZP);
			cpu.pc += 2;
			cpu.cycles += 3;
			
			break;
		// SBC zp
		case 0xE5:
			sbc(ZP);
			cpu.pc += 2;
			cpu.cycles += 3;
			break;
		// INC zp
		case 0xE6:
			ramWriteByte(ZP_ADDR, inc(ZP));
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// INX
		case 0xE8:
			cpu.x = inc(cpu.x);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// SBC imm
		case 0xE9:
			sbc(IMM);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// NOP
		case 0xEA:
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// CPX abs
		case 0xEC:
			cmp(cpu.x, ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// SBC abs
		case 0xED:
			sbc(ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// INC abs
		case 0xEE:
			ramWriteByte(ABS_ADDR, inc(ABS));
			cpu.pc += 3;
			cpu.cycles += 6;
			break;
		// BEQ
		case 0xF0:
			branch((cpu.p & Z_FLAG) != 0);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// SBC (ind), Y
		case 0xF1:
			sbc(INDIR_INDEX_Y);
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// SBC zp, X
		case 0xF5:
			sbc(ZP_INDEX(cpu.x));
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// INC zp, X
		case 0xF6:
			ramWriteByte(ZP_INDEX_ADDR(cpu.x), ZP_INDEX(cpu.x));
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		// SED
		case 0xF8:
			cpu.p |= D_FLAG;
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// SBC abs, Y
		case 0xF9:
			sbc(ABS_INDEX(cpu.y));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// SBC abs, X
		case 0xFD:
			sbc(ABS_INDEX(cpu.x));
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// INC abs, X
		case 0xFE:
			ramWriteByte(ABS_INDEX_ADDR(cpu.x), inc(ABS_INDEX(cpu.x)));
			cpu.pc += 3;
			cpu.cycles += 7;
			break;
		default:
			printf("unimplemented opcode %02X\n", opcode);
			#ifndef DEBUG
				cpuDumpState();
			#endif
			exit(1);
	}

	return 0;
}
