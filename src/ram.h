#ifndef RAM_H
#define RAM_H

#include <stdint.h>

extern uint8_t cpuRAM[0x10000];

#define ADDR16(addr) (uint16_t)((uint16_t)ramReadByte(addr) | (uint16_t)((ramReadByte(addr+1))<<8))

// ram writing functions to do specific things for like ppu registers and whatever
void ramWriteByte(uint16_t addr, uint8_t byte);
uint8_t ramReadByte(uint16_t addr);

#endif
