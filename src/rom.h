#ifndef ROM_H
#define ROM_H

#include <stdint.h>
#include <stddef.h>

extern uint8_t* prgROM;
extern uint8_t* chrROM;
extern size_t prgSize;
extern size_t chrSize;

void setMapper(uint16_t id);

// probably should get better names for these that aren't so similar to the ram functions
extern void (*romWriteByte)(uint16_t addr, uint8_t byte);
extern uint8_t (*romReadByte)(uint16_t addr);

extern void (*chrWriteByte)(uint16_t addr, uint8_t byte);
extern uint8_t (*chrReadByte)(uint16_t addr);

#endif
