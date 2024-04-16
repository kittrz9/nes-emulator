#ifndef ROM_H
#define ROM_H

#include <stdint.h>

// storing these outside of RAM in case I want to implement bank switching later on
extern uint8_t* prgROM;
extern uint8_t* chrROM;

#endif
