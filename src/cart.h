#ifndef CART_H
#define CART_H

#include <stdint.h>

// storing these outside of RAM in case I want to implement bank switching later on
extern uint8_t* prgROM;
extern uint8_t* chrROM;

#endif
