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
	cpu.nmi = 1;
	return;
}

void push(uint8_t byte) {
	ramWriteByte(0x100 + cpu.s, byte);
	--cpu.s;
	++cpu.cycles;
	return;
}

uint8_t pop(void) {
	++cpu.s;
	cpu.cycles += 2;
	return ramReadByte(0x100 + cpu.s);
}

void branch(uint8_t cond, int8_t offset) {
	if(cond) {
		uint8_t oldPage = cpu.pc >> 8;
		cpu.pc += (int8_t)offset;
		if(cpu.pc>>8 != oldPage) { ++cpu.cycles; }
		++cpu.cycles;
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

	// cycle counts for unofficial opcodes probably wont be accurate

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
			if(opcode == 0x4C || opcode == 0x6C) {
				// exception for jmp
				++cpu.cycles;
			} else {
				cpu.cycles += 2;
			}
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
					uint8_t startPage = addr >> 8;
					addr += cpu.y;
					++cpu.pc;
					cpu.cycles += 3;
					if(opcode == 0x91) {
						++cpu.cycles; // hardcoded exception for sta
					} else if(startPage != addr >> 8) {
						++cpu.cycles;
					}
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
				uint8_t startPage = ABS_ADDR >> 8;
				if(opcode == 0x99 || startPage != addr >> 8) {
					++cpu.cycles;
				}
				cpu.pc += 2;
				cpu.cycles += 2;
			}
			break;
		case 7:
			uint8_t startPage = ABS_ADDR >> 8;
			if(instrType > 1 && opcode > 0x80 && opcode < 0xC0) {
				// absolute y indexed
				addr = ABS_INDEX_ADDR(cpu.y);
			} else {
				// absolute x indexed
				addr = ABS_INDEX_ADDR(cpu.x);
				if(instrType == 2) {
					++cpu.cycles;
				}
			}
			cpu.pc += 2;
			cpu.cycles += 2;
			if(opcode == 0x9D || opcode == 0x99) {
				++cpu.cycles; // hardcoded exception for sta
			} else if((instrType != 2 || opcode == 0xBE) && startPage != addr >> 8) {
				++cpu.cycles;
			}
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
					cpu.cycles += 2;
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
					cpu.cycles += 2; // 6 total
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
					cpu.cycles = 6; // hardcoded exception, should be fixed
					//cpu.cycles += 4;
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
					//cpu.cycles += 4;
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
					//ramWriteByte(addr, cpu.y & ((addr >> 8)+1));
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
							cpu.cycles += 2;
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
								//ramWriteByte(addr, cpu.x & (((addr>>8) + 1)&0xFF));
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
			if(addrMode == 0x0B) {
				switch(opcode >> 5) {
					case 0:
					case 1:
						// anc
						and_a(ramReadByte(addr));
						setFlag(C_FLAG, cpu.p & N_FLAG);
						break;
					case 2:
						// alr
						and_a(ramReadByte(addr));
						cpu.a = lsr(cpu.a);
						break;
					case 3:
						// arr
						and_a(ramReadByte(addr));
						cpu.a = ror(cpu.a);
						setFlag(C_FLAG, cpu.a & (1<<6));
						setFlag(V_FLAG, ((cpu.a >> 6)&1) ^ ((cpu.a >> 5)&1));
						break;
					case 4:
						// xaa
						// probably inaccurate
						transfer(&cpu.x, cpu.a);
						and_a(ramReadByte(addr));
						break;
					case 5:
						// lax
						load(&cpu.a, ramReadByte(addr));
						transfer(&cpu.x, cpu.a);
						break;
					case 6:
						// axs
						uint8_t value = ramReadByte(addr);
						setFlag(C_FLAG, (cpu.x&cpu.a) >= value);
						cpu.x = (cpu.x&cpu.a) - value;
						setFlag(N_FLAG, cpu.x & 0x80);
						setFlag(Z_FLAG, cpu.x == 0);
						break;
					case 7:
						// sbc
						sbc(ramReadByte(addr));
						break;
				}
			} else {
				switch(opcode >> 5) {
					case 0:
						// slo
						ramWriteByte(addr, asl(ramReadByte(addr)));
						ora(ramReadByte(addr));
						break;
					case 1:
						// rla
						ramWriteByte(addr, rol(ramReadByte(addr)));
						and_a(ramReadByte(addr));
						break;
					case 2:
						// sre
						ramWriteByte(addr, lsr(ramReadByte(addr)));
						eor(ramReadByte(addr));
						break;
					case 3:
						// rra
						ramWriteByte(addr, ror(ramReadByte(addr)));
						adc(ramReadByte(addr));
						break;
					case 4:
						// sax
						ramWriteByte(addr, cpu.a & cpu.x);
						break;
					case 5:
						// lax
						load(&cpu.a, ramReadByte(addr));
						transfer(&cpu.x, cpu.a);
						break;
					case 6:
						// dcp
						ramWriteByte(addr, dec(ramReadByte(addr)));
						cmp(cpu.a, ramReadByte(addr));
						break;
					case 7:
						// isc
						ramWriteByte(addr, inc(ramReadByte(addr)));
						sbc(ramReadByte(addr));
						break;
				}
			}
			break;
	}

	if(!(cpu.p & I_FLAG) && cpu.irq == 0) {
		push((cpu.pc & 0xFF00) >> 8);
		push(cpu.pc & 0xFF);
		push((cpu.p & ~(B_FLAG)) | 0x20);
		//cpu.cycles -= 3; // account for pushes to the stack, unsure if this is accurate
		cpu.p |= I_FLAG;
		cpu.pc = ADDR16(IRQ_VECTOR);
	}
	cpu.irq = 1;

	if(cpu.nmi == 0) {
		push((cpu.pc & 0xFF00) >> 8);
		push(cpu.pc & 0xFF);
		push((cpu.p & ~(B_FLAG)) | 0x20);
		cpu.p |= I_FLAG;
		cpu.pc = ADDR16(NMI_VECTOR);
	}
	cpu.nmi = 1;

	/*static uint8_t cycles[256][5] = {0};
	if(cycles[opcode][cpu.cycles - 2] == 0) {
		cycles[opcode][cpu.cycles - 2] = 1;
		printf("%02X: %i\n", opcode, cpu.cycles);
	}*/

	return 0;
}
