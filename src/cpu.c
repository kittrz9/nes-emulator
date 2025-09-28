#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>

#include "ram.h"
#include "apu.h"

cpu_t cpu;

#define ARG8 ramReadByte(cpu.pc)
#define ARG16 ADDR16(cpu.pc)
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

void setFlag(uint8_t flag, uint8_t value) {
	if(value) {
		cpu.p |= flag;
	} else {
		cpu.p &= ~(flag);
	}
}

void cpuInit(void) {
	cpu.pc = ADDR16(RST_VECTOR);
	cpu.s = 0xFD;
	cpu.p |= I_FLAG;
	cpu.irq = 1;
	return;
}

void push(uint8_t byte) {
	ramWriteByte(0x100 + cpu.s, byte);
	--cpu.s;
	return;
}

uint8_t pop(void) {
	++cpu.s;
	return ramReadByte(0x100 + cpu.s);
}

void branch(uint8_t cond, int8_t offset) {
	if(cond) {
		uint8_t oldPage = cpu.pc >> 8;
		cpu.pc += (int8_t)offset;
		if(cpu.pc>>8 != oldPage) { cpu.cycles += 1; }
		cpu.cycles += 1;
	}
	return;
}

void cmp(uint8_t reg, uint8_t byte) {
	setFlag(C_FLAG, reg >= byte);
	setFlag(Z_FLAG, reg == byte);
	setFlag(N_FLAG, (reg - byte) & 0x80);
	return;
}

void bit(uint8_t byte) {
	setFlag(Z_FLAG, (byte & cpu.a) == 0);
	setFlag(V_FLAG, byte & V_FLAG);
	setFlag(N_FLAG, byte & N_FLAG);
	return;
}

void ora(uint8_t byte) {
	cpu.a |= byte;
	setFlag(Z_FLAG, cpu.a == 0);
	setFlag(N_FLAG, (cpu.a & 0x80) != 0);
	return;
}

void and_a(uint8_t byte) {
	cpu.a &= byte;
	setFlag(Z_FLAG, cpu.a == 0);
	setFlag(N_FLAG, (cpu.a & 0x80) != 0);
	return;
}

void eor(uint8_t byte) {
	cpu.a = cpu.a ^ byte;
	setFlag(Z_FLAG, cpu.a == 0);
	setFlag(N_FLAG, (cpu.a & 0x80) != 0);
}

// return result so it can be put into ram or A
uint8_t asl(uint8_t byte) {
	setFlag(C_FLAG, byte & 0x80);
	byte <<= 1;
	setFlag(Z_FLAG, byte == 0);
	setFlag(N_FLAG, byte & 0x80);
	return byte;
}

uint8_t lsr(uint8_t byte) {
	setFlag(C_FLAG, byte & 0x01);
	byte >>= 1;
	setFlag(Z_FLAG, byte == 0);
	setFlag(N_FLAG, byte & 0x80);
	return byte;
}

uint8_t ror(uint8_t byte) {
	uint8_t carry = cpu.p & C_FLAG;
	setFlag(C_FLAG, byte & 0x01);
	byte >>= 1;
	byte |= carry << 7;
	setFlag(Z_FLAG, byte == 0);
	setFlag(N_FLAG, (byte & 0x80) != 0);
	return byte;
}

uint8_t rol(uint8_t byte) {
	uint8_t carry = cpu.p & C_FLAG;
	setFlag(C_FLAG, byte & 0x80);
	byte <<= 1;
	byte |= carry;
	setFlag(Z_FLAG, byte == 0);
	setFlag(N_FLAG, (byte & 0x80) != 0);
	return byte;
}

void adc(uint8_t byte) {
	uint16_t tmp = cpu.a + byte + (cpu.p & C_FLAG);
	uint8_t result = tmp & 0xFF;
	//setFlag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
	setFlag(V_FLAG, (result ^ cpu.a) & (result ^ byte) & 0x80);
	cpu.a = result;
	setFlag(C_FLAG, tmp > 255);
	setFlag(Z_FLAG, cpu.a == 0);
	setFlag(N_FLAG, (cpu.a & 0x80) != 0);
	return;
}

void sbc(uint8_t byte) {
	int16_t tmp = cpu.a - byte - !(cpu.p & C_FLAG);
	//setFlag(V_FLAG, ((cpu.a & 0x80) ^ (byte & 0x80)) != (tmp & 0x80));
	uint8_t result = tmp & 0xFF;
	setFlag(V_FLAG, (result ^ cpu.a) & (result ^ ~byte) & 0x80);
	cpu.a = tmp & 0xFF;
	setFlag(C_FLAG, !(tmp < 0));
	setFlag(Z_FLAG, cpu.a == 0);
	setFlag(N_FLAG, (cpu.a & 0x80) != 0);
}

uint8_t dec(uint8_t byte) {
	--byte;
	setFlag(Z_FLAG, byte == 0);
	setFlag(N_FLAG, (byte & 0x80) != 0);
	return byte;
}

uint8_t inc(uint8_t byte) {
	++byte;
	setFlag(Z_FLAG, byte == 0);
	setFlag(N_FLAG, (byte & 0x80) != 0);
	return byte;
}

// using pointers here since I don't have to worry about affecting ram
void load(uint8_t* reg, uint8_t byte) {
	*reg = byte;
	setFlag(Z_FLAG, byte == 0);
	setFlag(N_FLAG, (byte & 0x80) != 0);
	return;
}

void transfer(uint8_t* reg, uint8_t byte) {
	*reg = byte;
	setFlag(Z_FLAG, byte == 0);
	setFlag(N_FLAG, (byte & 0x80) != 0);
	return;
}

uint8_t cpuStep(void) {
	uint8_t opcode = ramReadByte(cpu.pc);
	//cpuDumpState();
	++cpu.pc;
	cpu.cycles += 2;

	// https://www.nesdev.org/wiki/CPU_unofficial_opcodes

	// decoding the instructions like this might be slightly slower than just having the massive switch statement
	// since I think the switch statement would just compile to a huge jump table
	// but I haven't actually done any testing to see if that's the case
	// it also seems to have broken the brk instruction slightly according to accuracycoin
	// it says it fails to do an irq, but then if you do the test again it passes
	// so idk maybe it's something to do with the cycle timings that probably aren't fully accurate with this
	// doesn't seem to affect any games and everything else seems fine
	uint16_t addr;
	uint8_t addrMode = opcode & 0x1F;
	uint8_t instrType = opcode & 3;
	switch(addrMode >> 2) {
		case 0:
			if(opcode == 0x20) {
				// absolute (jsr)
				addr = ABS_ADDR;
				cpu.pc += 2;
			} else if(instrType % 2 == 0) {
				if(opcode >= 0x80) {
					// immediate
					addr = cpu.pc;
					++cpu.pc;
				} else {
					// implicit
					addr = 0;
				}
			} else {
				// x indexed indirect
				//addr = INDEX_INDIR_X_ADDR;
				addr = ramReadByte((ARG8 + cpu.x) & 0xFF);
				addr |= ramReadByte((ARG8 + cpu.x + 1) & 0xFF)<<8;
				//ramWriteByte(addr, cpu.a);
				++cpu.pc;
				cpu.cycles += 4;
			}
			break;
		case 1:
			// zero page
			addr = ramReadByte(cpu.pc);
			++cpu.pc;
			++cpu.cycles;
			break;
		case 2:
			if(instrType % 2 == 0) {
				// implicit
				addr = 0;
			} else {
				// immediate
				addr = cpu.pc;
				++cpu.pc;
			}
			break;
		case 3:
			// absolute
			addr = ABS_ADDR;
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		case 4:
			switch(instrType) {
				case 0:
					// relative
					addr = cpu.pc;
					++cpu.pc;
					break;
				case 1:
				case 3:
					// indirect y indexed
					//addr = INDIR_INDEX_Y_ADDR;
					addr = ramReadByte(ARG8);
					addr |= (ramReadByte((ARG8+1)&0xFF))<<8;
					addr += cpu.y;
					++cpu.pc;
					cpu.cycles += 3;
					break;
				case 2:
					// implicit (stp)
					addr = 0;
					break;
			}
			break;
		case 5:
			if(instrType > 1 && opcode > 0x80 && opcode < 0xC0) {
				// zero page y indexed
				addr = ZP_INDEX_ADDR(cpu.y);
			} else {
				// zero page x indexed
				addr = ZP_INDEX_ADDR(cpu.x);
			}
			++cpu.pc;
			cpu.cycles += 2;
			break;
		case 6:
			if(instrType % 2 == 0) {
				// implicit
				addr = 0;
			} else {
				// absolute y indexed
				addr = ABS_INDEX_ADDR(cpu.y);
				cpu.pc += 2;
				cpu.cycles += 2;
			}
			break;
		case 7:
			if(instrType == 2 && opcode > 0x80 && opcode < 0xC0) {
				// absolute y indexed
				addr = ABS_INDEX_ADDR(cpu.y);
			} else {
				// absolute x indexed
				addr = ABS_INDEX_ADDR(cpu.x);
			}
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
	}
	switch(instrType) {
		case 0:
			// control instructions
			// not really sure what the pattern is to most of these
			if(((opcode & 0x1F) == 0x14 || (opcode & 0x1F) == 0x1C) && ((opcode & 0x80) == 0)) {
				// nop
				break;
			}
			switch(opcode) {
				case 0x04:
				case 0x0C:
				case 0x44:
				case 0x64:
				case 0x80:
					// nop
					break;
				case 0x00:
					// brk
					++cpu.pc;
					push((cpu.pc & 0xFF00) >> 8);
					push(cpu.pc & 0xFF);
					push(cpu.p | B_FLAG | 0x20);
					cpu.p |= I_FLAG;
					cpu.pc = ADDR16(IRQ_VECTOR);
					cpu.cycles += 5;
					break;
				case 0x08:
					// php
					push(cpu.p | B_FLAG | 0x20);
					break;
				case 0x20:
					// jsr
					--cpu.pc;
					push((cpu.pc & 0xFF00) >> 8);
					push(cpu.pc & 0xFF);
					cpu.pc = addr;
					break;
				case 0x28:
					// plp
					cpu.p = pop();
					break;
				case 0x24:
				case 0x2C:
					// bit
					bit(ramReadByte(addr));
					break;
				case 0x40:
					// rti
					cpu.p = pop();
					cpu.pc = pop();
					cpu.pc |= pop()<<8;
					cpu.cycles += 4;
					break;
				case 0x48:
					// pha
					push(cpu.a);
					break;
				case 0x4C:
					// jmp
					cpu.pc = addr;
					break;
				case 0x60:
					// rts
					cpu.pc = pop();
					cpu.pc |= pop()<<8;
					++cpu.pc;
					cpu.cycles += 4;
					break;
				case 0x68:
					// pla
					cpu.a = pop();
					setFlag(Z_FLAG, cpu.a == 0);
					setFlag(N_FLAG, (cpu.a & 0x80) != 0);
					break;
				case 0x6C:
					// jmp indirect
					uint16_t addr1 = addr;
					if(((addr+1) & 0xFF) == 0xFF) {
						addr1 -= 0x100;
					}
					uint16_t addr2 = ramReadByte(addr1) | ramReadByte(addr1+1)<<8;
					if((addr1 & 0xFF) == 0xFF) {
						addr2 = ramReadByte(addr1) | ramReadByte(addr1&0xFF00)<<8;
					} else {
						addr2 = ramReadByte(addr1) | ramReadByte(addr1+1)<<8;
					}
					cpu.pc = addr2;
					cpu.cycles += 2;
					break;
				case 0x84:
				case 0x8C:
				case 0x94:
					// sty
					ramWriteByte(addr, cpu.y);
					break;
				case 0x88:
					// dey
					cpu.y = dec(cpu.y);
					break;
				case 0x98:
					// tya
					transfer(&cpu.a, cpu.y);
					break;
				case 0x9C:
					// shy
					// unimplemented
					break;
				case 0xA0:
				case 0xA4:
				case 0xAC:
				case 0xB4:
				case 0xBC:
					// ldy
					load(&cpu.y, ramReadByte(addr));
					break;
				case 0xA8:
					// tay
					transfer(&cpu.y, cpu.a);
					break;
				case 0xC0:
				case 0xC4:
				case 0xCC:
					// cpy
					cmp(cpu.y, ramReadByte(addr));
					break;
				case 0xC8:
					// iny
					cpu.y = inc(cpu.y);
					break;
				case 0xE0:
				case 0xE4:
				case 0xEC:
					// cpx
					cmp(cpu.x, ramReadByte(addr));
					break;
				case 0xE8:
					// inx
					cpu.x = inc(cpu.x);
					break;

				// branches
				case 0x10:
					// bpl
					branch((cpu.p & N_FLAG) == 0, ramReadByte(addr));
					break;
				case 0x30:
					// bmi
					branch((cpu.p & N_FLAG) != 0, ramReadByte(addr));
					break;
				case 0x50:
					// bvc
					branch((cpu.p & V_FLAG) == 0, ramReadByte(addr));
					break;
				case 0x70:
					// bvs
					branch((cpu.p & V_FLAG) != 0, ramReadByte(addr));
					break;
				case 0x90:
					// bcc
					branch((cpu.p & C_FLAG) == 0, ramReadByte(addr));
					break;
				case 0xB0:
					// bcs
					branch((cpu.p & C_FLAG) != 0, ramReadByte(addr));
					break;
				case 0xD0:
					// bne
					branch((cpu.p & Z_FLAG) == 0, ramReadByte(addr));
					break;
				case 0xF0:
					// beq
					branch((cpu.p & Z_FLAG) != 0, ramReadByte(addr));
					break;


				// cpu flag stuff
				case 0x18:
					// clc
					cpu.p &= ~(C_FLAG);
					break;
				case 0x38:
					// sec
					cpu.p |= C_FLAG;
					break;
				case 0x58:
					// cli
					cpu.p &= ~(I_FLAG);
					break;
				case 0x78:
					// sei
					cpu.p |= I_FLAG;
					break;
				case 0xB8:
					// clv
					cpu.p &= ~(V_FLAG);
					break;
				case 0xD8:
					// cld
					cpu.p &= ~(D_FLAG);
					break;
				case 0xF8:
					// sed
					cpu.p |= D_FLAG;
					break;
			}
			break;
		case 1:
			// alu instructions
			switch(opcode >> 4) {
				case 0:
				case 1:
					// ora
					ora(ramReadByte(addr));
					break;
				case 2:
				case 3:
					// and
					and_a(ramReadByte(addr));
					break;
				case 4:
				case 5:
					// eor
					eor(ramReadByte(addr));
					break;
				case 6:
				case 7:
					// adc
					adc(ramReadByte(addr));
					break;
				case 8:
				case 9:
					// sta
					if((opcode & 0x1F) != 0x09) {
						ramWriteByte(addr, cpu.a);
					}
					break;
				case 0xA:
				case 0xB:
					// lda
					load(&cpu.a, ramReadByte(addr));
					break;
				case 0xC:
				case 0xD:
					// cmp
					cmp(cpu.a, ramReadByte(addr));
					break;
				case 0xE:
				case 0xF:
					// sbc
					sbc(ramReadByte(addr));
					break;
			}
			break;
		case 2:
			// rmw instructions
			if(addrMode == 0x12 || (addrMode == 0x02 && opcode < 0x80)) {
				// stp
			} else if(addrMode == 0x1A && (opcode < 0x80 || opcode > 0xC0)) {
				// nop
			} else {
				switch(opcode >> 4) {
					case 0x0:
					case 0x1:
						// asl
						if((opcode & 0x1F) == 0x0A) {
							cpu.a = asl(cpu.a);
						} else {
							ramWriteByte(addr, asl(ramReadByte(addr)));
							cpu.cycles += 2;
						}
						break;
					case 0x2:
					case 0x3:
						// rol
						if((opcode & 0x1F) == 0x0A) {
							cpu.a = rol(cpu.a);
						} else {
							ramWriteByte(addr, rol(ramReadByte(addr)));
							cpu.cycles += 2;
						}
						break;
					case 0x4:
					case 0x5:
						// lsr
						if((opcode & 0x1F) == 0x0A) {
							cpu.a = lsr(cpu.a);
						} else {
							ramWriteByte(addr, lsr(ramReadByte(addr)));
							cpu.cycles += 2;
						}
						break;
					case 0x6:
					case 0x7:
						// ror
						if((opcode & 0x1F) == 0x0A) {
							cpu.a = ror(cpu.a);
						} else {
							ramWriteByte(addr, ror(ramReadByte(addr)));
						}
						break;
					case 0x8:
					case 0x9:
						switch(opcode & 0x1F) {
							case 0x02:
								// nop
								break;
							case 0x06:
							case 0x0E:
							case 0x16:
								// stx
								ramWriteByte(addr, cpu.x);
								break;
							case 0x0A:
								// txa
								transfer(&cpu.a, cpu.x);
								break;
							case 0x1A:
								// txs
								cpu.s = cpu.x;
								break;
							case 0x1E:
								// shx (unimplemented)
								break;
						}
						break;
					case 0xA:
					case 0xB:
						switch(opcode & 0x1F) {
							case 0x02:
							case 0x06:
							case 0x0E:
							case 0x16:
							case 0x1E:
								// ldx
								load(&cpu.x, ramReadByte(addr));
								break;
							case 0x0A:
								// tax
								transfer(&cpu.x, cpu.a);
								break;
							case 0x1A:
								// tsx
								transfer(&cpu.x, cpu.s);
								break;
						}
						break;
					case 0xC:
					case 0xD:
						switch(opcode & 0x1F) {
							case 0x02:
							case 0x1A:
								// nop
								break;
							case 0x06:
							case 0x0E:
							case 0x16:
							case 0x1E:
								// dec
								ramWriteByte(addr, dec(ramReadByte(addr)));
								cpu.cycles += 2;
								break;
							case 0x0A:
								// dex
								cpu.x = dec(cpu.x);
								break;
						}
						break;
					case 0xE:
					case 0xF:
						switch(opcode & 0x1F) {
							case 0x02:
							case 0x0A:
							case 0x1A:
								// nop
								break;
							case 0x06:
							case 0x0E:
							case 0x16:
							case 0x1E:
								// inc
								ramWriteByte(addr, inc(ramReadByte(addr)));
								cpu.cycles += 2;
								break;
						}
						break;
				}
			}
			break;
		case 3:
			// unofficial opcodes
			break;
	}

/*	// https://www.masswerk.at/6502/6502_instruction_set.html
	// https://www.nesdev.org/obelisk-6502-guide/reference.html
	switch(opcode) {
		// BRK
		case 0x00:
			cpu.pc += 2;
			push((cpu.pc & 0xFF00) >> 8);
			push(cpu.pc & 0xFF);
			push(cpu.p | B_FLAG | 0x20);
			cpu.p |= I_FLAG;
			cpu.pc = ADDR16(IRQ_VECTOR);
			cpu.cycles += 7;
			break;
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
			push(cpu.p | B_FLAG | 0x20);
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
		// ORA (ind), Y
		case 0x11:
			ora(INDIR_INDEX_Y);
			cpu.pc += 2;
			cpu.cycles += 5;
			break;
		// ORA zp, X
		case 0x15:
			ora(ZP_INDEX(cpu.x));
			cpu.pc += 2;
			cpu.cycles += 4;
			break;
		// ASL zp, X
		case 0x16:
			ramWriteByte(ZP_INDEX_ADDR(cpu.x), asl(ZP_INDEX(cpu.x)));
			cpu.pc += 2;
			cpu.cycles += 6;
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
			cpu.cycles += 3;
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
			cpu.pc = pop();
			cpu.pc |= pop()<<8;
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
		// EOR abs
		case 0x4D:
			eor(ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
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
		// CLI
		case 0x58:
			cpu.p &= ~(I_FLAG);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// EOR abs, Y
		case 0x59:
			eor(ABS_INDEX(cpu.y));
			cpu.pc += 3;
			cpu.cycles += 4;
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
			setFlag(Z_FLAG, cpu.a == 0);
			setFlag(N_FLAG, (cpu.a & 0x80) != 0);
			cpu.pc += 1;
			cpu.cycles += 4;
			break;
		// ADC imm
		case 0x69:
			adc(IMM);
			cpu.pc += 2;
			cpu.cycles += 2;
			break;
		// ROR A
		case 0x6A:
			cpu.a = ror(cpu.a);
			cpu.pc += 1;
			cpu.cycles += 2;
			break;
		// JMP (ind)
		case 0x6C: {
			uint16_t addr1 = ramReadByte(cpu.pc+1) | ramReadByte(cpu.pc+2)<<8;
			if(((cpu.pc+2) & 0xFF) == 0xFF) {
				addr1 -= 0x100;
			}
			uint16_t addr2 = ramReadByte(addr1) | ramReadByte(addr1+1)<<8;
			if((addr1 & 0xFF) == 0xFF) {
				addr2 = ramReadByte(addr1) | ramReadByte(addr1&0xFF00)<<8;
			} else {
				addr2 = ramReadByte(addr1) | ramReadByte(addr1+1)<<8;
			}
			cpu.pc = addr2;
			cpu.cycles += 5;
			break;
		}
		// ADC abs
		case 0x6D:
			adc(ABS);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// ROR abs
		case 0x6E:
			ramWriteByte(ABS_ADDR, ror(ABS));
			cpu.pc += 3;
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
			ramWriteByte(ZP_INDEX_ADDR(cpu.x), ror(ZP_INDEX(cpu.x)));
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
		case 0x81: {
			// I probably should've just made the addressing mode stuff functions instead of macros to avoid having to debug such a nightmare
			// this fixes the only issue with indexed indirect that got detected by nestest
			uint16_t addr = ramReadByte(ARG8 + cpu.x);
			addr |= ramReadByte((ARG8 + cpu.x + 1) & 0xFF)<<8;
			ramWriteByte(addr, cpu.a);
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		}
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
			cpu.cycles += 4;
			break;
		// STA abs
		case 0x8D:
			ramWriteByte(ABS_ADDR, cpu.a);
			cpu.pc += 3;
			cpu.cycles += 4;
			break;
		// STX abs
		case 0x8E:
			ramWriteByte(ABS_ADDR, cpu.x);
			cpu.pc += 3;
			cpu.cycles += 4;
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
			cpu.cycles += 6;
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
		case 0xA1: {
			uint16_t addr = ramReadByte((ARG8 + cpu.x) & 0xFF);
			addr |= ramReadByte((ARG8 + cpu.x + 1) & 0xFF)<<8;
			load(&cpu.a, ramReadByte(addr));
			cpu.pc += 2;
			cpu.cycles += 6;
			break;
		}
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
		case 0xB1: {
			uint16_t addr = ramReadByte(ARG8);
			addr |= (ramReadByte((ARG8+1)&0xFF))<<8;
			addr += cpu.y;
			load(&cpu.a, ramReadByte(addr));
			cpu.pc += 2;
			cpu.cycles += 5; // figure out what the docs mean when they say to add 1 cycle when it "crosses a page boundary" here
			break;
		}
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
			load(&cpu.x, ZP_INDEX(cpu.y));
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
			cpu.cycles += 4;
			break;
		// DEC zp, X
		case 0xD6:
			ramWriteByte(ZP_INDEX_ADDR(cpu.x), dec(ZP_INDEX(cpu.x)));
			cpu.pc += 2;
			cpu.cycles += 6;
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
			cpu.cycles += 2;
			break;
		// SBC (ind, X)
		case 0xE1:
			sbc(INDEX_INDIR_X);
			cpu.pc += 2;
			cpu.cycles += 6;
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
			ramWriteByte(ZP_INDEX_ADDR(cpu.x), inc(ZP_INDEX(cpu.x)));
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
			cpu.pc += 1;
			cpu.cycles += 0;
			//exit(1);
	}
*/
	//apuFrameCheck(cpu.cycles - lastCycles);
	/*if(cpu.cycles % 2 == 0) {
		apuStep();
	}*/

	if(!(cpu.p & I_FLAG) && cpu.irq == 0) {
		//printf("IRQ!!! %04X\n", cpu.pc);
		//cpuDumpState();
		push((cpu.pc & 0xFF00) >> 8);
		push(cpu.pc & 0xFF);
		push((cpu.p & ~(B_FLAG)) | 0x20);
		cpu.p |= I_FLAG;
		cpu.pc = ADDR16(IRQ_VECTOR);
		//printf("%04X\n", cpu.pc);
		cpu.irq = 1;
	}

	return 0;
}
